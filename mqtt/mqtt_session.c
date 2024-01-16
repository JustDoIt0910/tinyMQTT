//
// Created by zr on 23-4-20.
//
#include "mqtt_session.h"
#include "mqtt_io_context.h"
#include "net/mqtt_tcp_conn.h"
#include "base/mqtt_util.h"
#include "mqtt_tasks.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern void mqtt_subscribe(tmq_broker_t* broker, tmq_str_t client_id, tmq_subscribe_pkt* subscribe_pkt);
extern void mqtt_unsubscribe(tmq_broker_t* broker, tmq_str_t client_id, tmq_unsubscribe_pkt* unsubscribe_pkt);
extern void on_mqtt_subscribe_response(tiny_mqtt* mqtt, tmq_suback_pkt * suback_pkt);
extern void on_mqtt_unsubscribe_response(tiny_mqtt* mqtt, tmq_unsuback_pkt* unsuback_pkt);

static sending_packet_t* sending_packet_new(tmq_packet_type type, void* pkt, uint16_t packet_id)
{
    sending_packet_t* sending_pkt = malloc(sizeof(sending_packet_t));
    if(!sending_pkt) fatal_error("malloc() error: out of memory");
    sending_pkt->packet_id = packet_id;
    sending_pkt->packet.packet_type = type;
    sending_pkt->packet.packet_ptr = pkt;
    sending_pkt->next = NULL;
    sending_pkt->send_time = 0;
    return sending_pkt;
}

static void acknowledge(tmq_session_t* session, uint16_t packet_id, tmq_packet_type type, int qos)
{
    message_store_t* store = session->message_store;
    sending_packet_t* next = store->acknowledge_and_next(store, session, packet_id, type, qos);
    if(!next) return;
    tmq_any_packet_t send = next->packet;
    if(send.packet_type == MQTT_PUBLISH)
        send.packet_ptr = tmq_publish_pkt_clone(next->packet.packet_ptr);
    else send.packet_ptr = tmq_pubrel_pkt_clone(next->packet.packet_ptr);
    tmq_session_send_packet(session, &send, 0);
}

static void resend_messages(void* arg)
{
    tmq_session_t* session = arg;
    message_store_t* store = session->message_store;
    sending_packet_t* sending_pkt = store->sending_queue_head;
    while(sending_pkt != NULL && sending_pkt != store->pending_pointer)
    {
        tmq_any_packet_t send = sending_pkt->packet;
        if(send.packet_type == MQTT_PUBLISH)
            send.packet_ptr = tmq_publish_pkt_clone(sending_pkt->packet.packet_ptr);
        else send.packet_ptr = tmq_pubrel_pkt_clone(sending_pkt->packet.packet_ptr);
        tmq_session_send_packet(session, &send, 0);
        sending_pkt = sending_pkt->next;
    }
}

static void session_cleanup_(tmq_ref_counted_t* obj)
{
    tmq_session_t* session = (tmq_session_t*) obj;
    tmq_str_free(session->client_id);
    tmq_str_free(session->will_topic);
    tmq_str_free(session->will_message);
    tmq_map_free(session->subscriptions);
    tmq_map_free(session->qos2_packet_ids);
    free(session->message_store);
    // TODO cleanup message_store
}

tmq_session_t* tmq_session_new(void* upstream, new_message_cb on_new_message, close_cb on_close, tmq_tcp_conn_t* conn,
                               char* client_id, uint8_t clean_session, uint16_t keep_alive, char* will_topic,
                               char* will_message, uint8_t will_qos, uint8_t will_retain, uint8_t max_inflight,
                               message_store_t* message_store)
{
    tmq_session_t* session = malloc(sizeof(tmq_session_t));
    if(!session) fatal_error("malloc() error: out of memory");
    bzero(session, sizeof(tmq_session_t));
    session->cleaner = session_cleanup_;
    session->upstream = upstream;
    session->on_new_message = on_new_message;
    session->on_close = on_close;
    session->state = OPEN;
    session->clean_session = clean_session;
    session->conn = TCP_CONN_SHARE(conn);
    session->client_id = tmq_str_new(client_id);
    session->keep_alive = keep_alive;
    session->last_pkt_ts = time_now();
    session->inflight_window_size = max_inflight;
    if(will_topic)
    {
        session->will_topic = tmq_str_new(will_topic);
        session->will_message = tmq_str_new(will_message);
        session->will_qos = will_qos;
        session->will_retain = will_retain;
    }
    unsigned int rand_seed = time(NULL);
    session->next_packet_id = rand_r(&rand_seed) % UINT16_MAX;

    session->message_store = message_store;

    tmq_map_str_init(&session->subscriptions, uint8_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_map_32_init(&session->qos2_packet_ids, uint8_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    
    return session;
}

void tmq_session_start(tmq_session_t* session)
{
    resend_messages(session);
}

void tmq_session_close(tmq_session_t* session, int force_clean)
{
    if(session->state == OPEN)
        session->state = CLOSED;
    if(session->conn)
        TCP_CONN_RELEASE(session->conn);
    if(session->on_close)
        session->on_close(session->upstream, session, force_clean);
}

void tmq_session_resume(tmq_session_t* session, tmq_tcp_conn_t* conn, uint16_t keep_alive, char* will_topic,
                        char* will_message, uint8_t will_qos, uint8_t will_retain)
{
    session->conn = TCP_CONN_SHARE(conn);
    session->state = OPEN;
    session->last_pkt_ts = time_now();
    session->keep_alive = keep_alive;
    session->will_topic = NULL;
    session->will_message = NULL;
    if(will_topic)
    {
        tmq_str_free(session->will_topic);
        tmq_str_free(session->will_message);
        session->will_topic = tmq_str_new(will_topic);
        session->will_message = tmq_str_new(will_message);
        session->will_qos = will_qos;
        session->will_retain = will_retain;
    }
}

void tmq_session_handle_subscribe(tmq_session_t* session, tmq_subscribe_pkt* subscribe_pkt)
{
    session->last_pkt_ts = time_now();
    topic_filter_qos* tf = tmq_vec_begin(subscribe_pkt->topics);
    for(; tf != tmq_vec_end(subscribe_pkt->topics); tf++)
        tmq_map_put(session->subscriptions, tf->topic_filter, tf->qos);

    mqtt_subscribe((tmq_broker_t*) session->upstream, session->client_id, subscribe_pkt);
}

void tmq_session_handle_unsubscribe(tmq_session_t* session, tmq_unsubscribe_pkt* unsubscribe_pkt)
{
    session->last_pkt_ts = time_now();
    for(tmq_str_t* tf = tmq_vec_begin(unsubscribe_pkt->topics); tf != tmq_vec_end(unsubscribe_pkt->topics); tf++)
        tmq_map_erase(session->subscriptions, *tf);

    mqtt_unsubscribe((tmq_broker_t*) session->upstream, session->client_id,unsubscribe_pkt);
}

void tmq_session_handle_suback(tmq_session_t* session, tmq_suback_pkt* suback_pkt)
{
    session->last_pkt_ts = time_now();
    on_mqtt_subscribe_response((tiny_mqtt*) session->upstream, suback_pkt);
}

void tmq_session_handle_unsuback(tmq_session_t* session, tmq_unsuback_pkt* unsuback_pkt)
{
    session->last_pkt_ts = time_now();
    on_mqtt_unsubscribe_response((tiny_mqtt*) session->upstream, unsuback_pkt);
}

void tmq_session_handle_publish(tmq_session_t* session, tmq_publish_pkt* publish_pkt)
{
    session->last_pkt_ts = time_now();
    tmq_message message = {
            .qos = PUBLISH_QOS(publish_pkt->flags),
            .message = tmq_str_new(publish_pkt->payload)
    };
    /* for qos2 message, check if it is a redelivery */
    if(PUBLISH_QOS(publish_pkt->flags) == 2)
    {
        /* if this is the first time that receive this publish message,
         * store the packet id and deliver this message */
        if(tmq_map_get(session->qos2_packet_ids, publish_pkt->packet_id) == NULL)
            tmq_map_put(session->qos2_packet_ids, publish_pkt->packet_id, 1);
            /* if it is a redelivered message, just discard it. */
        else
        {
            tmq_publish_pkt_cleanup(publish_pkt);
            return;
        }
    }
    session->on_new_message(session->upstream, session, tmq_str_new(publish_pkt->topic),
                            &message, PUBLISH_RETAIN(publish_pkt->flags));
    tmq_publish_pkt_cleanup(publish_pkt);
}

void tmq_session_handle_pingreq(tmq_session_t* session) {session->last_pkt_ts = time_now();}

void tmq_session_handle_pingresp(tmq_session_t* session) {session->last_pkt_ts = time_now();}

void tmq_session_handle_puback(tmq_session_t* session, tmq_puback_pkt* puback_pkt)
{
    session->last_pkt_ts = time_now();
    acknowledge(session, puback_pkt->packet_id, MQTT_PUBLISH, 1);
}

void tmq_session_handle_pubrec(tmq_session_t* session, tmq_pubrec_pkt* pubrec_pkt)
{
    session->last_pkt_ts = time_now();
    acknowledge(session, pubrec_pkt->packet_id, MQTT_PUBLISH, 2);
    tmq_pubrel_pkt* pubrel_pkt = malloc(sizeof(tmq_pubrel_pkt));
    pubrel_pkt->packet_id = pubrec_pkt->packet_id;

    message_store_t* store = session->message_store;
    sending_packet_t* sending_pkt = sending_packet_new(MQTT_PUBREL, pubrel_pkt, pubrel_pkt->packet_id);
    if(store->store_message(store, session, sending_pkt))
    {
        tmq_any_packet_t pkt = {
                .packet_type = MQTT_PUBREL,
                .packet_ptr = tmq_pubrel_pkt_clone(pubrel_pkt)
        };
        sending_pkt->send_time = time_now();
        tmq_session_send_packet(session, &pkt, 0);
    }
}

void tmq_session_handle_pubrel(tmq_session_t* session, tmq_pubrel_pkt* pubrel_pkt)
{
    session->last_pkt_ts = time_now();
    tmq_map_erase(session->qos2_packet_ids, pubrel_pkt->packet_id);
}

void tmq_session_handle_pubcomp(tmq_session_t* session, tmq_pubcomp_pkt* pubcomp_pkt)
{
    session->last_pkt_ts = time_now();
    acknowledge(session, pubcomp_pkt->packet_id, MQTT_PUBREL, -1);
}

void tmq_session_send_packet(tmq_session_t* session, tmq_any_packet_t* pkt, int queue)
{
    if(!queue)
    {
        tmq_send_any_packet(session->conn, pkt);
        tmq_any_pkt_cleanup(pkt);
    }
    else
    {
        tmq_io_context_t* context = session->conn->io_context;
        packet_send_task* send_task = malloc(sizeof(packet_send_task));
        send_task->conn = TCP_CONN_SHARE(session->conn);
        send_task->pkt = *pkt;
        tmq_mailbox_push(&context->packet_sending_tasks, send_task);
    }
}

void tmq_session_publish(tmq_session_t* session, tmq_str_t topic, tmq_str_t payload, uint8_t qos, uint8_t retain)
{
    tmq_publish_pkt* publish_pkt = malloc(sizeof(tmq_publish_pkt));
    bzero(publish_pkt, sizeof(tmq_publish_pkt));
    publish_pkt->topic = tmq_str_new(topic);
    publish_pkt->payload = tmq_str_new(payload);
    publish_pkt->flags |= retain;

    tmq_any_packet_t pkt = {
            .packet_type = MQTT_PUBLISH,
            .packet_ptr = publish_pkt
    };
    int send_now = 1;
    // if qos = 0, fire and forget
    if(qos > 0)
    {
        publish_pkt->packet_id = session->next_packet_id;
        session->next_packet_id = session->next_packet_id == UINT16_MAX ? 0 : session->next_packet_id + 1;
        publish_pkt->flags |= (qos << 1);

        tmq_publish_pkt* stored_pkt = tmq_publish_pkt_clone(publish_pkt);

        message_store_t* store = session->message_store;
        sending_packet_t* sending_pkt = sending_packet_new(MQTT_PUBLISH, stored_pkt, publish_pkt->packet_id);
        send_now = store->store_message(store, session, sending_pkt);
        if(send_now) sending_pkt->send_time = time_now();
    }

    if(send_now) tmq_session_send_packet(session, &pkt, 0);
    else tmq_publish_pkt_cleanup(publish_pkt);
}

void tmq_session_subscribe(tmq_session_t* session, const char* topic_filter, uint8_t qos)
{
    topic_list topics = tmq_vec_make(topic_filter_qos);
    topic_filter_qos topic = {
            .topic_filter = tmq_str_new(topic_filter),
            .qos = qos
    };
    tmq_vec_push_back(topics, topic);

    tmq_subscribe_pkt* subscribe_pkt = malloc(sizeof(tmq_subscribe_pkt));
    subscribe_pkt->packet_id = session->next_packet_id;
    subscribe_pkt->topics = topics;
    session->next_packet_id = session->next_packet_id == UINT16_MAX ? 0 : session->next_packet_id + 1;
    tmq_any_packet_t pkt = {
            .packet_type = MQTT_SUBSCRIBE,
            .packet_ptr = subscribe_pkt
    };
    tmq_session_send_packet(session, &pkt, 0);
}

void tmq_session_unsubscribe(tmq_session_t* session, const char* topic_filter)
{
    str_vec topics = tmq_vec_make(tmq_str_t);
    tmq_vec_push_back(topics, tmq_str_new(topic_filter));

    tmq_unsubscribe_pkt* unsubscribe_pkt = malloc(sizeof(tmq_unsubscribe_pkt));
    unsubscribe_pkt->packet_id =  session->next_packet_id;
    unsubscribe_pkt->topics = topics;
    session->next_packet_id = session->next_packet_id == UINT16_MAX ? 0 : session->next_packet_id + 1;
    tmq_any_packet_t pkt = {
            .packet_type = MQTT_UNSUBSCRIBE,
            .packet_ptr = unsubscribe_pkt
    };
    tmq_session_send_packet(session, &pkt, 0);
}

void tmq_session_set_publish_finish_callback(tmq_session_t* session, publish_finish_cb cb)
{
    if(!session) return;
    session->on_publish_finish = cb;
}

void tmq_session_free(tmq_session_t* session)
{
    session_cleanup_((tmq_ref_counted_t*) session);
    free(session);
}
