//
// Created by zr on 23-6-2.
//
#include "mqtt_io_group.h"
#include "net/mqtt_tcp_conn.h"
#include "mqtt/mqtt_session.h"
#include "base/mqtt_util.h"
#include "mqtt_broker.h"
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

/* called when closing a tcp conn */
static void tcp_conn_cleanup(tmq_tcp_conn_t* conn, void* arg)
{
    tmq_io_group_t* group = conn->group;
    tcp_conn_ctx* ctx = conn->context;
    assert(ctx != NULL);

    /* IN_SESSION state means that the client closed the connection without sending
     * a disconnect packet, we have to clean the session in the broker. */
    if(ctx->conn_state == IN_SESSION)
    {
        ctx->conn_state = NO_SESSION;
        tmq_broker_t* broker = group->broker;
        session_ctl ctl = {
                .op = SESSION_CLOSE,
                .context.session = ctx->upstream.session
        };
        pthread_mutex_lock(&broker->session_ctl_lk);
        tmq_vec_push_back(broker->session_ctl_reqs, ctl);
        pthread_mutex_unlock(&broker->session_ctl_lk);

        tmq_notifier_notify(&broker->session_ctl_notifier);
    }

    char conn_name[50];
    tmq_tcp_conn_id(conn, conn_name, sizeof(conn_name));
    tmq_map_erase(group->tcp_conns, conn_name);
    release_ref(conn);

    release_ref(conn);
}

static void tcp_checkalive(void* arg)
{
    tmq_io_group_t* group = arg;

    int64_t now = time_now();
    tmq_vec(tmq_tcp_conn_t*) timeout_conns = tmq_vec_make(tmq_tcp_conn_t*);
    tmq_map_iter_t it = tmq_map_iter(group->tcp_conns);
    for(; tmq_map_has_next(it); tmq_map_next(group->tcp_conns, it))
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
        tmq_tcp_conn_close(get_ref(*conn_it));
    tmq_vec_free(timeout_conns);
}

static void mqtt_keepalive(void* arg)
{
    tmq_io_group_t *group = arg;

    int64_t now = time_now();
    tmq_vec(tmq_tcp_conn_t*) timeout_conns = tmq_vec_make(tmq_tcp_conn_t*);
    tmq_map_iter_t it = tmq_map_iter(group->tcp_conns);
    for(; tmq_map_has_next(it); tmq_map_next(group->tcp_conns, it))
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
        tmq_tcp_conn_close(get_ref(*conn_it));
    tmq_vec_free(timeout_conns);
}

extern void tcp_conn_ctx_cleanup(void* arg);

static void handle_new_connection(void* arg)
{
    tmq_io_group_t* group = arg;

    tmq_vec(tmq_socket_t) conns = tmq_vec_make(tmq_socket_t);
    pthread_mutex_lock(&group->pending_conns_lk);
    tmq_vec_swap(conns, group->pending_conns);
    pthread_mutex_unlock(&group->pending_conns_lk);

    for(tmq_socket_t* it = tmq_vec_begin(conns); it != tmq_vec_end(conns); it++)
    {
        tmq_tcp_conn_t* conn = tmq_tcp_conn_new(&group->loop, group, *it, &group->broker->codec);
        conn->close_cb = tcp_conn_cleanup;
        conn->state = CONNECTED;

        tcp_conn_ctx* conn_ctx = malloc(sizeof(tcp_conn_ctx));
        tmq_vec_init(&conn_ctx->pending_packets, tmq_any_packet_t);
        conn_ctx->upstream.broker = group->broker;
        conn_ctx->conn_state = NO_SESSION;
        conn_ctx->parsing_ctx.state = PARSING_FIXED_HEADER;
        conn_ctx->last_msg_time = time_now();
        tmq_tcp_conn_set_context(conn, conn_ctx, tcp_conn_ctx_cleanup);

        char conn_name[50];
        tmq_tcp_conn_id(conn, conn_name, sizeof(conn_name));
        tmq_map_put(group->tcp_conns, conn_name, get_ref(conn));
        assert(conn->ref_cnt == 1);

        tlog_info("new connection [%s] group=%p thread=%lu", conn_name, group, mqtt_tid);
    }
    tmq_vec_free(conns);
}

static void handle_new_session(void* arg)
{
    tmq_io_group_t *group = arg;

    connect_resp_list resps = tmq_vec_make(session_connect_resp);
    pthread_mutex_lock(&group->connect_resp_lk);
    tmq_vec_swap(resps, group->connect_resp);
    pthread_mutex_unlock(&group->connect_resp_lk);

    session_connect_resp* resp = tmq_vec_begin(resps);
    for(; resp != tmq_vec_end(resps); resp++)
    {
        tcp_conn_ctx* conn_ctx = resp->conn->context;

        /* if the client sent a disconnect packet or closed the tcp connection before the
         * session-establishing procedure complete, we need to close the session in the broker */
        if((resp->return_code == CONNECTION_ACCEPTED) &&
        (conn_ctx->conn_state == NO_SESSION) || (resp->conn->state != CONNECTED))
        {
            tmq_broker_t* broker = group->broker;
            session_ctl ctl = {
                    .op = conn_ctx->conn_state == NO_SESSION ? SESSION_DISCONNECT : SESSION_CLOSE,
                    .context.session = resp->session
            };
            pthread_mutex_lock(&broker->session_ctl_lk);
            tmq_vec_push_back(broker->session_ctl_reqs, ctl);
            pthread_mutex_unlock(&broker->session_ctl_lk);

            tmq_notifier_notify(&broker->session_ctl_notifier);
            release_ref(resp->conn);
            continue;
        }

        if(resp->return_code == CONNECTION_ACCEPTED)
        {
            conn_ctx->upstream.session = resp->session;
            conn_ctx->conn_state = IN_SESSION;
            tlog_info("connect success[session=%p]", resp->session);
        }
        else
            tlog_info("connect failed, return_code=%x", resp->return_code);
        tmq_connack_pkt pkt = {
                .return_code = resp->return_code,
                .ack_flags = resp->session_present
        };
        send_connack_packet(resp->conn, &pkt);
        release_ref(resp->conn);
    }
    tmq_vec_free(resps);
}

static void send_packets(void* arg)
{
    tmq_io_group_t *group = arg;

    packet_send_list packets = tmq_vec_make(packet_send_req);
    pthread_mutex_lock(&group->sending_packets_lk);
    tmq_vec_swap(packets, group->sending_packets);
    pthread_mutex_unlock(&group->sending_packets_lk);

    packet_send_req* req = tmq_vec_begin(packets);
    for(; req != tmq_vec_end(packets); req++)
    {
        tcp_conn_ctx* ctx = req->conn->context;
        if(req->conn->state == CONNECTED && ctx->conn_state == IN_SESSION)
            send_any_packet(req->conn, &req->pkt);
        tmq_any_pkt_cleanup(&req->pkt);
        release_ref(req->conn);
    }

    tmq_vec_free(packets);
}

void tmq_io_group_init(tmq_io_group_t* group, tmq_broker_t* broker)
{
    group->broker = broker;
    tmq_event_loop_init(&group->loop);
    tmq_map_str_init(&group->tcp_conns, tmq_tcp_conn_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);

    tmq_timer_t* timer = tmq_timer_new(SEC_MS(MQTT_TCP_CHECKALIVE_INTERVAL), 1, tcp_checkalive, group);
    group->tcp_checkalive_timer = tmq_event_loop_add_timer(&group->loop, timer);
    timer = tmq_timer_new(SEC_MS(1), 1, mqtt_keepalive, group);
    group->mqtt_keepalive_timer = tmq_event_loop_add_timer(&group->loop, timer);

    if(pthread_mutex_init(&group->pending_conns_lk, NULL))
        fatal_error("pthread_mutex_init() error %d: %s", errno, strerror(errno));
    if(pthread_mutex_init(&group->connect_resp_lk, NULL))
        fatal_error("pthread_mutex_init() error %d: %s", errno, strerror(errno));
    if(pthread_mutex_init(&group->sending_packets_lk, NULL))
        fatal_error("pthread_mutex_init() error %d: %s", errno, strerror(errno));

    tmq_vec_init(&group->pending_conns, tmq_socket_t);
    tmq_vec_init(&group->connect_resp, session_connect_resp);
    tmq_vec_init(&group->sending_packets, packet_send_req);

    tmq_notifier_init(&group->new_conn_notifier, &group->loop, handle_new_connection, group);
    tmq_notifier_init(&group->connect_resp_notifier, &group->loop, handle_new_session, group);
    tmq_notifier_init(&group->sending_packets_notifier, &group->loop, send_packets, group);
}

static void* io_group_thread_func(void* arg)
{
    tmq_io_group_t* group = (tmq_io_group_t*) arg;
    tmq_event_loop_run(&group->loop);

    /* clean up */
    /* free all connections in the connection map */
    tmq_map_iter_t it = tmq_map_iter(group->tcp_conns);
    for(; tmq_map_has_next(it); tmq_map_next(group->tcp_conns, it))
        tmq_tcp_conn_free(*(tmq_tcp_conn_t**)it.second);
    tmq_map_free(group->tcp_conns);

    /* close pending conns in the pending list */
    tmq_socket_t* fd_it = tmq_vec_begin(group->pending_conns);
    for(; fd_it != tmq_vec_end(group->pending_conns); fd_it++)
        close(*fd_it);
    tmq_vec_free(group->pending_conns);
    tmq_vec_free(group->connect_resp);
    packet_send_req* req = tmq_vec_begin(group->sending_packets);
    for(; req != tmq_vec_end(group->sending_packets); req++)
        tmq_any_pkt_cleanup(&req->pkt);
    tmq_vec_free(group->sending_packets);

    tmq_notifier_destroy(&group->new_conn_notifier);
    tmq_notifier_destroy(&group->connect_resp_notifier);
    tmq_notifier_destroy(&group->sending_packets_notifier);

    pthread_mutex_destroy(&group->pending_conns_lk);
    pthread_mutex_destroy(&group->connect_resp_lk);
    pthread_mutex_destroy(&group->sending_packets_lk);

    tmq_event_loop_destroy(&group->loop);
}

void tmq_io_group_run(tmq_io_group_t* group)
{
    if(pthread_create(&group->io_thread, NULL, io_group_thread_func, group) != 0)
        fatal_error("pthread_create() error %d: %s", errno, strerror(errno));
}

void tmq_io_group_stop(tmq_io_group_t* group)
{
    tmq_event_loop_cancel_timer(&group->loop, group->tcp_checkalive_timer);
    tmq_event_loop_cancel_timer(&group->loop, group->mqtt_keepalive_timer);
    tmq_event_loop_quit(&group->loop);
}
