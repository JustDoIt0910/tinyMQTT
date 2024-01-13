//
// Created by just do it on 2024/1/10.
//
#include <stdlib.h>
#include <string.h>
#include "store/mqtt_msg_store.h"
#include "db/mqtt_db.h"
#include "thrdpool/thrdpool.h"
#include "mqtt/mqtt_broker.h"
#include "mqtt/mqtt_io_context.h"
#include "mqtt/mqtt_session.h"
#include "tlog.h"

static int memory_store_store_message(message_store_t* store, tmq_session_t* session, sending_packet_t* sending_pkt)
{
    int send_now = 1;
    /* if the sending queue is empty */
    if(!store->sending_queue_tail)
        store->sending_queue_head = store->sending_queue_tail = sending_pkt;
    else
    {
        store->sending_queue_tail->next = sending_pkt;
        store->sending_queue_tail = sending_pkt;
    }
    /* if the number of inflight packets less than the inflight window size,
     * this packet can be sent immediately */
    if(session->inflight_packets < session->inflight_window_size)
        session->inflight_packets++;
        /* otherwise, it must wait for sending in the sending_queue */
    else
    {
        send_now = 0;
        if(!store->pending_pointer)
            store->pending_pointer = sending_pkt;
    }
    store->total_size++;
    return send_now;
}

static sending_packet_t* memory_store_acknowledge(message_store_t* store, tmq_session_t* session, uint16_t packet_id,
                                                tmq_packet_type type, int qos)
{
    int is_valid_ack = 0;
    sending_packet_t** p = &store->sending_queue_head;
    int cnt = 0;
    while(*p && cnt++ < session->inflight_packets)
    {
        if((*p)->packet_id != packet_id || (*p)->packet.packet_type != type)
        {
            p = &((*p)->next);
            continue;
        }
        if(type == MQTT_PUBLISH)
        {
            tmq_publish_pkt* pkt = (*p)->packet.packet_ptr;
            if(PUBLISH_QOS(pkt->flags) != qos)
            {
                p = &((*p)->next);
                continue;
            }
            if(session->on_publish_finish)
                session->on_publish_finish(session->upstream, pkt->packet_id, qos);
        }
        sending_packet_t* next = (*p)->next;
        sending_packet_t* remove = *p;
        *p = next;
        if(store->sending_queue_tail == remove)
            store->sending_queue_tail = store->sending_queue_head ? (sending_packet_t*) p: NULL;
        tmq_any_pkt_cleanup(&remove->packet);
        free(remove);
        session->inflight_packets--;
        store->total_size--;
        is_valid_ack = 1;
        break;
    }
    sending_packet_t* next = NULL;
    if(store->pending_pointer && is_valid_ack)
    {
        next = store->pending_pointer;
        store->pending_pointer->send_time = time_now();
        store->pending_pointer = store->pending_pointer->next;
        session->inflight_packets++;
    }
    return next;
}

typedef struct store_context
{
    mongoc_client_pool_t* mongodb_pool;
    tmq_session_t* session;
    sending_packet_t* packets;
    int n_packets;
    tmq_mailbox_t* receipt_mailbox;
} store_context;

typedef struct fetch_context
{
    mongoc_client_pool_t* mongodb_pool;
    tmq_session_t* session;
    int limit;
    tmq_mailbox_t* receipt_mailbox;
} fetch_context;

static void store_message_done(void* arg);
static void store_messages_in_thread_pool(void* arg)
{
    store_context* ctx = arg;
    mongoc_client_pool_t* pool = ctx->mongodb_pool;
    mongoc_client_t* mongo_client = mongoc_client_pool_pop(pool);
    store_messages_to_mongodb(mongo_client, ctx->session->client_id, ctx->packets, ctx->n_packets);
    tmq_db_return_receipt_t* receipt = malloc(sizeof(tmq_db_return_receipt_t));
    receipt->receipt_routine = store_message_done;
    receipt->arg = ctx->session;
    tmq_mailbox_push(ctx->receipt_mailbox, receipt);
    free(ctx);
}

typedef struct fetch_result
{
    tmq_session_t* session;
    int n_messages;
    sending_packet_t* messages_head;
    sending_packet_t* messages_tail;
} fetch_result;

static void fetch_message_done(void* arg);
static void fetch_messages_in_thread_pool(void* arg)
{
    fetch_context* ctx = arg;
    mongoc_client_pool_t* pool = ctx->mongodb_pool;
    mongoc_client_t* mongo_client = mongoc_client_pool_pop(pool);
    fetch_result* result = malloc(sizeof(fetch_result));
    result->n_messages = fetch_messages_from_mongodb(mongo_client, ctx->session->client_id, ctx->limit,
                                                     &result->messages_head, &result->messages_tail);
    tmq_db_return_receipt_t* receipt = malloc(sizeof(tmq_db_return_receipt_t));
    result->session = ctx->session;
    receipt->receipt_routine = fetch_message_done;
    receipt->arg = result;
    tmq_mailbox_push(ctx->receipt_mailbox, receipt);
    free(ctx);
}

static void initiate_storing_task(tmq_session_t* session, message_store_mongodb_t* mongodb_store)
{
    tmq_broker_t* broker = (tmq_broker_t*)session->upstream;
    tmq_io_context_t* io_context = session->conn->io_context;

    CLEAR_NEED_STORE(mongodb_store->states);
    SET_STORING(mongodb_store->states);
    thrdpool_t* pool = broker->thread_pool;
    store_context* ctx = malloc(sizeof(store_context));
    ctx->mongodb_pool = broker->mongodb_pool;
    ctx->session = SESSION_SHARE(session);
    ctx->packets = mongodb_store->buffer_queue_head;
    ctx->n_packets = mongodb_store->buffer_size;
    ctx->receipt_mailbox = &io_context->thread_pool_return_receipts;
    struct thrdpool_task task = {
            .routine = store_messages_in_thread_pool,
            .context = ctx
    };
    thrdpool_schedule(&task, pool);
    mongodb_store->total_size -= mongodb_store->buffer_size;
    mongodb_store->buffer_size = 0;
    mongodb_store->buffer_queue_head = NULL;
    mongodb_store->buffer_queue_tail = &mongodb_store->buffer_queue_head;
    SET_STORED_MESSAGES(mongodb_store->states);
}

static void initiate_fetching_task(tmq_session_t* session, message_store_mongodb_t* mongodb_store)
{
    tmq_broker_t* broker = (tmq_broker_t*)session->upstream;
    tmq_io_context_t* io_context = session->conn->io_context;

    SET_FETCHING(mongodb_store->states);
    CLEAR_NEED_FETCH(mongodb_store->states);
    thrdpool_t* pool = broker->thread_pool;
    fetch_context* ctx = malloc(sizeof(fetch_context));
    ctx->mongodb_pool = broker->mongodb_pool;
    ctx->receipt_mailbox = &io_context->thread_pool_return_receipts;
    ctx->session = SESSION_SHARE(session);
    ctx->limit = 50;
    struct thrdpool_task task = {
            .routine = fetch_messages_in_thread_pool,
            .context = ctx
    };
    thrdpool_schedule(&task, pool);
}

void store_message_done(void* arg)
{
    tmq_session_t* session = arg;
    message_store_mongodb_t* store = (message_store_mongodb_t*)session->message_store;
    CLEAR_STORING(store->states);
    if(NEED_STORE(store->states))
        initiate_storing_task(session, store);
    else if(NEED_FETCH(store->states))
        initiate_fetching_task(session, store);
    SESSION_RELEASE(session);
}

static void use_buffered_messages(message_store_mongodb_t* mongodb_store);
static void continue_send_messages(tmq_session_t* session, message_store_mongodb_t* mongodb_store);
void fetch_message_done(void* arg)
{
    fetch_result* result = arg;
    tmq_session_t* session = result->session;
    message_store_mongodb_t* store = (message_store_mongodb_t*)session->message_store;
    CLEAR_FETCHING(store->states);
    if(result->n_messages > 0)
    {
        store->total_size += result->n_messages;
        store->sending_queue_head = result->messages_head;
        store->sending_queue_tail = result->messages_tail;
        store->pending_pointer = NULL;
        continue_send_messages(session, store);
    }
    else
    {
        CLEAR_STORED_MESSAGES(store->states);
        if(store->buffer_size > 0)
        {
            use_buffered_messages(store);
            continue_send_messages(session, store);
        }
        else
            CLEAR_BUFFERED_MESSAGES(store->states);
    }
    if(NEED_STORE(store->states))
    {
        if(store->buffer_size > 0)
            initiate_storing_task(session, store);
        else
            CLEAR_NEED_STORE(store->states);
    }
    else if(NEED_FETCH(store->states))
        initiate_fetching_task(session, store);
    SESSION_RELEASE(result->session);
    free(result);
}

static int mongodb_store_store_message(message_store_t* store, tmq_session_t* session, sending_packet_t* sending_pkt)
{
    int send_now = 0;
    sending_pkt->store_timestamp = session->store_timestamp++;
    message_store_mongodb_t* mongodb_store = (message_store_mongodb_t*)store;
    if((mongodb_store->total_size < mongodb_store->thresh / 2 && !HAS_BUFFERED_MESSAGES(mongodb_store->states)) ||
    sending_pkt->packet.packet_type == MQTT_PUBREL)
        send_now = memory_store_store_message(store, session, sending_pkt);
    else
    {
        mongodb_store->total_size++;
        mongodb_store->buffer_size++;
        *mongodb_store->buffer_queue_tail = sending_pkt;
        mongodb_store->buffer_queue_tail = &sending_pkt->next;
        SET_BUFFERED_MESSAGES(mongodb_store->states);
        if(mongodb_store->total_size >= mongodb_store->thresh)
        {
            if(FETCHING(mongodb_store->states) || STORING(mongodb_store->states))
                SET_NEED_STORE(mongodb_store->states);
            else
                initiate_storing_task(session, mongodb_store);
        }
    }
    return send_now;
}

void use_buffered_messages(message_store_mongodb_t* mongodb_store)
{
    mongodb_store->sending_queue_head = mongodb_store->buffer_queue_head;
    mongodb_store->sending_queue_tail = (sending_packet_t*)mongodb_store->buffer_queue_tail;
    mongodb_store->pending_pointer = NULL;
    mongodb_store->buffer_queue_head = NULL;
    mongodb_store->buffer_queue_tail = &mongodb_store->buffer_queue_head;
    mongodb_store->buffer_size = 0;
    CLEAR_BUFFERED_MESSAGES(mongodb_store->states);
}

void continue_send_messages(tmq_session_t* session, message_store_mongodb_t* mongodb_store)
{
    sending_packet_t* p = mongodb_store->sending_queue_head;
    while(p && session->inflight_packets < session->inflight_window_size)
    {
        tmq_any_packet_t send = p->packet;
        send.packet_ptr = tmq_publish_pkt_clone(p->packet.packet_ptr);
        tmq_session_send_packet(session, &send, 0);
        session->inflight_packets++;
        p = p->next;
    }
    if(p)
        mongodb_store->pending_pointer = p;
}

static sending_packet_t* mongodb_store_acknowledge(message_store_t* store, tmq_session_t* session, uint16_t packet_id,
                                                 tmq_packet_type type, int qos)
{
    sending_packet_t* next = memory_store_acknowledge(store, session, packet_id, type, qos);
    message_store_mongodb_t* mongodb_store = (message_store_mongodb_t*)store;
    if((mongodb_store->total_size - mongodb_store->buffer_size == 0) && HAS_BUFFERED_MESSAGES(mongodb_store->states))
    {
        if(HAS_STORED_MESSAGES(mongodb_store->states))
        {
            if(FETCHING(mongodb_store->states) || STORING(mongodb_store->states))
                SET_NEED_FETCH(mongodb_store->states);
            else
                initiate_fetching_task(session, mongodb_store);
        }
        else
        {
            use_buffered_messages(mongodb_store);
            continue_send_messages(session, mongodb_store);
        }
    }
    return next;
}

message_store_t* tmq_message_store_memory_new()
{
    message_store_memory_t* store = malloc(sizeof(message_store_memory_t));
    bzero(store, sizeof(message_store_memory_t));
    store->acknowledge_and_next = memory_store_acknowledge;
    store->store_message = memory_store_store_message;
    return (message_store_t*)store;
}

message_store_t* tmq_message_store_mongodb_new(uint16_t trigger_thresh)
{
    message_store_mongodb_t* store = malloc(sizeof(message_store_mongodb_t));
    bzero(store, sizeof(message_store_mongodb_t));
    store->thresh = trigger_thresh;
    store->buffer_queue_tail = &store->buffer_queue_head;
    store->acknowledge_and_next = mongodb_store_acknowledge;
    store->store_message = mongodb_store_store_message;
    return (message_store_t*)store;
}