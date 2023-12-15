//
// Created by zr on 23-6-2.
//
#include "mqtt_io_context.h"
#include "net/mqtt_tcp_conn.h"
#include "mqtt/mqtt_session.h"
#include "base/mqtt_util.h"
#include "mqtt_broker.h"
#include "mqtt_tasks.h"
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

extern void handle_session_req(void* arg);

/* called when closing a tcp conn */
static void tcp_conn_cleanup(tmq_tcp_conn_t* conn, void* arg)
{
    tmq_io_context_t* context = conn->io_context;
    tcp_conn_ctx* conn_ctx = conn->context;
    assert(conn_ctx != NULL);

    /* IN_SESSION state means that the client closed the connection without sending
     * a disconnect packet, we have to clean the session in the broker. */
    if(conn_ctx->conn_state == IN_SESSION)
    {
        conn_ctx->conn_state = NO_SESSION;
        tmq_broker_t* broker = context->broker;

        session_req req = {
                .op = SESSION_FORCE_CLOSE,
                .session = conn_ctx->upstream.session
        };
        session_task_ctx* task_ctx = malloc(sizeof(session_task_ctx));
        task_ctx->broker = broker;
        task_ctx->req = req;
        tmq_executor_post(&broker->executor, handle_session_req, task_ctx,1);
    }

    char conn_name[50];
    tmq_tcp_conn_id(conn, conn_name, sizeof(conn_name));
    tmq_map_erase(context->tcp_conns, conn_name);
    release_ref((tmq_ref_counted_t*) conn);
}

static void tcp_checkalive(void* arg)
{
    tmq_io_context_t* context = arg;

    int64_t now = time_now();
    tmq_vec(tmq_tcp_conn_t*) timeout_conns = tmq_vec_make(tmq_tcp_conn_t*);
    tmq_map_iter_t it = tmq_map_iter(context->tcp_conns);
    for(; tmq_map_has_next(it); tmq_map_next(context->tcp_conns, it))
    {
        tmq_tcp_conn_t* conn = *(tmq_tcp_conn_t**) (it.second);
        tcp_conn_ctx* ctx = conn->context;
        if((ctx->conn_state == NO_SESSION && now - ctx->last_msg_time > SEC_US(MQTT_CONNECT_MAX_PENDING)) ||
           (ctx->conn_state != NO_SESSION && now - ctx->last_msg_time > SEC_US(MQTT_TCP_MAX_IDLE)))
            tmq_vec_push_back(timeout_conns, conn);
    }
    /* do remove after iteration to prevent iterator failure */
    tmq_tcp_conn_t** conn_it = tmq_vec_begin(timeout_conns);
    for(; conn_it != tmq_vec_end(timeout_conns); conn_it++)
        tmq_tcp_conn_force_close(*conn_it);
    tmq_vec_free(timeout_conns);
}

static void mqtt_keepalive(void* arg)
{
    tmq_io_context_t *context = arg;

    int64_t now = time_now();
    tmq_vec(tmq_tcp_conn_t*) timeout_conns = tmq_vec_make(tmq_tcp_conn_t*);
    tmq_map_iter_t it = tmq_map_iter(context->tcp_conns);
    for(; tmq_map_has_next(it); tmq_map_next(context->tcp_conns, it))
    {
        tmq_tcp_conn_t* conn = *(tmq_tcp_conn_t**) (it.second);
        tcp_conn_ctx* ctx = conn->context;
        if(ctx->conn_state != IN_SESSION)
            continue;
        tmq_session_t* session = ctx->upstream.session;
        if(!session->keep_alive)
            continue;
        if(now - session->last_pkt_ts >= (int64_t) SEC_US(session->keep_alive * 1.5))
        {
            tlog_info("client[%s] is down", session->client_id);
            tmq_vec_push_back(timeout_conns, conn);
        }
    }
    /* do remove after iteration to prevent iterator failure */
    tmq_tcp_conn_t** conn_it = tmq_vec_begin(timeout_conns);
    for(; conn_it != tmq_vec_end(timeout_conns); conn_it++)
        tmq_tcp_conn_force_close(*conn_it);
    tmq_vec_free(timeout_conns);
}

extern void tcp_conn_broker_ctx_cleanup(void* arg);

static void new_tcp_connection_handler(void* owner, tmq_mail_t mail)
{
    tmq_io_context_t* context = owner;
    tmq_socket_t sock = (tmq_socket_t)(intptr_t)mail;

    tmq_tcp_conn_t* conn = tmq_tcp_conn_new(&context->loop, context, sock, &context->broker->codec);
    conn->on_close = tcp_conn_cleanup;
    conn->state = CONNECTED;

    tcp_conn_broker_ctx* conn_ctx = malloc(sizeof(tcp_conn_broker_ctx));
    tmq_vec_init(&conn_ctx->pending_packets, tmq_any_packet_t);
    conn_ctx->upstream.broker = context->broker;
    conn_ctx->conn_state = NO_SESSION;
    conn_ctx->parsing_ctx.state = PARSING_FIXED_HEADER;
    conn_ctx->last_msg_time = time_now();
    tmq_tcp_conn_set_context(conn, conn_ctx, tcp_conn_broker_ctx_cleanup);

    char conn_name[50];
    tmq_tcp_conn_id(conn, conn_name, sizeof(conn_name));
    get_ref((tmq_ref_counted_t*) conn);
    tmq_map_put(context->tcp_conns, conn_name, conn);
    assert(conn->ref_cnt == 3);

    //tlog_info("new connection [%s]", conn_name);
}

static void connect_complete_handler(void* owner, tmq_mail_t mail)
{
    tmq_io_context_t* context = owner;
    session_connect_resp* resp = mail;
    tcp_conn_ctx* conn_ctx = resp->conn->context;

    /* if the client sent a disconnect packet or closed the tcp connection before the
     * session-establishing procedure complete, we need to close the session in the broker */
    if((resp->return_code == CONNECTION_ACCEPTED) &&
    (conn_ctx->conn_state == NO_SESSION) || (resp->conn->state != CONNECTED))
    {
        tmq_broker_t* broker = context->broker;
        session_req req = {
                .op = (conn_ctx->conn_state == NO_SESSION)
                        ? SESSION_DISCONNECT
                        : SESSION_FORCE_CLOSE,
                .session = resp->session
        };
        session_task_ctx* ctx = malloc(sizeof(session_task_ctx));
        ctx->broker = broker;
        ctx->req = req;
        tmq_executor_post(&broker->executor, handle_session_req, ctx,1);

        release_ref((tmq_ref_counted_t*) resp->conn);
        return;
    }

    if(resp->return_code == CONNECTION_ACCEPTED)
    {
        conn_ctx->upstream.session = resp->session;
        conn_ctx->conn_state = IN_SESSION;
        tlog_info("connect success[%s]", resp->session->client_id);
    }
    else tlog_info("connect failed, return_code=%x", resp->return_code);
    tmq_connack_pkt pkt = {
            .return_code = resp->return_code,
            .ack_flags = resp->session_present
    };
    send_connack_packet(resp->conn, &pkt);
    if(resp->return_code == CONNECTION_ACCEPTED)
        tmq_session_start(resp->session);
    release_ref((tmq_ref_counted_t*) resp->conn);
    free(resp);
}

static void send_packet_handler(void* owner, tmq_mail_t mail)
{
    packet_send_task* send_task = mail;
    tmq_send_any_packet(send_task->conn, &send_task->pkt);
    tmq_any_pkt_cleanup(&send_task->pkt);
    release_ref((tmq_ref_counted_t*) send_task->conn);
    free(send_task);
}

static void broadcast_handler(void* owner, tmq_mail_t mail)
{
    broadcast_task_ctx* broadcast_task = mail;
    tmq_message* message = &broadcast_task->message;
    subscribe_info_t* info = tmq_vec_begin(broadcast_task->subscribers);
    for(; info != tmq_vec_end(broadcast_task->subscribers); info++)
    {
        tmq_session_t* session = info->session;
        uint8_t qos = info->qos < message->qos ? info->qos : message->qos;
        if(session->state == OPEN)
            tmq_session_publish(session, broadcast_task->topic, message->message, qos, broadcast_task->retain);
        else if(!session->clean_session)
            tmq_session_store_publish(session, broadcast_task->topic, message->message, qos, broadcast_task->retain);
    }
    tmq_str_free(broadcast_task->topic);
    tmq_str_free(broadcast_task->message.message);
    tmq_vec_free(broadcast_task->subscribers);
    free(broadcast_task);
}

static void mail_callback(void* arg)
{
    tmq_mailbox_t* mailbox = arg;
    tmq_mail_list_t mails = tmq_vec_make(tmq_mail_t);
    pthread_mutex_lock(&mailbox->lk);
    tmq_vec_swap(mails, mailbox->mailbox);
    pthread_mutex_unlock(&mailbox->lk);

    tmq_mail_t* mail = tmq_vec_begin(mails);
    for(; mail != tmq_vec_end(mails); mail++)
        mailbox->handler(mailbox->owner, *mail);
    tmq_vec_free(mails);
}

void tmq_mailbox_init(tmq_mailbox_t* mailbox, tmq_event_loop_t* loop, void* owner, mail_handler handler)
{
    tmq_notifier_init(&mailbox->notifier, loop, mail_callback, mailbox);
    pthread_mutex_init(&mailbox->lk, NULL);
    tmq_vec_init(&mailbox->mailbox, tmq_mail_t);
    mailbox->owner = owner;
    mailbox->handler = handler;
}

void tmq_mailbox_push(tmq_mailbox_t* mailbox, tmq_mail_t mail)
{
    pthread_mutex_lock(&mailbox->lk);
    tmq_vec_push_back(mailbox->mailbox, mail);
    pthread_mutex_unlock(&mailbox->lk);
    tmq_notifier_notify(&mailbox->notifier);
}

void tmq_io_context_init(tmq_io_context_t* context, tmq_broker_t* broker, int index)
{
    context->broker = broker;
    context->index = index;
    tmq_event_loop_init(&context->loop);
    tmq_map_str_init(&context->tcp_conns, tmq_tcp_conn_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);

//    tmq_timer_t* timer = tmq_timer_new(SEC_MS(MQTT_TCP_CHECKALIVE_INTERVAL), 1, tcp_checkalive, context);
//    context->tcp_checkalive_timer = tmq_event_loop_add_timer(&context->loop, timer);
//    timer = tmq_timer_new(SEC_MS(1), 1, mqtt_keepalive, context);
//    context->mqtt_keepalive_timer = tmq_event_loop_add_timer(&context->loop, timer);

//    if(pthread_mutex_init(&context->pending_conns_lk, NULL))
//        fatal_error("pthread_mutex_init() error %d: %s", errno, strerror(errno));
//    if(pthread_mutex_init(&context->connect_resp_lk, NULL))
//        fatal_error("pthread_mutex_init() error %d: %s", errno, strerror(errno));
//    if(pthread_mutex_init(&context->sending_packets_lk, NULL))
//        fatal_error("pthread_mutex_init() error %d: %s", errno, strerror(errno));
//
//    tmq_vec_init(&context->pending_tcp_conns, tmq_socket_t);
//    tmq_vec_init(&context->connect_resps, session_connect_resp);
//    tmq_vec_init(&context->sending_packets, packet_send_req);
//
//    tmq_notifier_init(&context->new_conn_notifier, &context->loop, handle_new_connection, context);
//    tmq_notifier_init(&context->connect_resp_notifier, &context->loop, handle_new_session, context);
//    tmq_notifier_init(&context->sending_packets_notifier, &context->loop, send_packets, context);

    tmq_mailbox_init(&context->pending_tcp_connections, &context->loop, context, new_tcp_connection_handler);
    tmq_mailbox_init(&context->mqtt_connect_responses, &context->loop, context, connect_complete_handler);
    tmq_mailbox_init(&context->packet_sending_tasks, &context->loop, context, send_packet_handler);
    tmq_mailbox_init(&context->broadcast_tasks, &context->loop, context, broadcast_handler);
}

static void* io_context_thread_func(void* arg)
{
    tmq_io_context_t* context = (tmq_io_context_t*) arg;
    tmq_event_loop_run(&context->loop);

    //TODO rewrite clean up logic

//    /* clean up */
//    /* free all connections in the connection map */
//    tmq_map_iter_t it = tmq_map_iter(context->tcp_conns);
//    for(; tmq_map_has_next(it); tmq_map_next(context->tcp_conns, it))
//        tmq_tcp_conn_free(*(tmq_tcp_conn_t**)it.second);
//    tmq_map_free(context->tcp_conns);
//
//    /* close pending conns in the pending list */
//    tmq_socket_t* fd_it = tmq_vec_begin(context->pending_tcp_conns);
//    for(; fd_it != tmq_vec_end(context->pending_tcp_conns); fd_it++)
//        close(*fd_it);
//    tmq_vec_free(context->pending_tcp_conns);
//    tmq_vec_free(context->connect_resps);
//
//    packet_send_req* req = tmq_vec_begin(context->sending_packets);
//    for(; req != tmq_vec_end(context->sending_packets); req++)
//        tmq_any_pkt_cleanup(&req->pkt);
//    tmq_vec_free(context->sending_packets);
//
//    tmq_notifier_destroy(&context->new_conn_notifier);
//    tmq_notifier_destroy(&context->connect_resp_notifier);
//    tmq_notifier_destroy(&context->sending_packets_notifier);
//
//    pthread_mutex_destroy(&context->pending_conns_lk);
//    pthread_mutex_destroy(&context->connect_resp_lk);
//    pthread_mutex_destroy(&context->sending_packets_lk);
//
//    tmq_event_loop_destroy(&context->loop);
}

void tmq_io_context_run(tmq_io_context_t* context)
{
    if(pthread_create(&context->io_thread, NULL, io_context_thread_func, context) != 0)
        fatal_error("pthread_create() error %d: %s", errno, strerror(errno));
}

void tmq_io_context_stop(tmq_io_context_t* context)
{
    tmq_event_loop_cancel_timer(&context->loop, context->tcp_checkalive_timer);
    tmq_event_loop_cancel_timer(&context->loop, context->mqtt_keepalive_timer);
    tmq_event_loop_quit(&context->loop);
}
