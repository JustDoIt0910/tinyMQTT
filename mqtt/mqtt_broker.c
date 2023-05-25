//
// Created by zr on 23-4-9.
//
#include "mqtt_broker.h"
#include "net/mqtt_tcp_conn.h"
#include "base/mqtt_util.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

/* mqtt io group */

static void remove_tcp_conn(tmq_tcp_conn_t* conn, void* arg)
{
    tmq_io_group_t* group = conn->group;
    tcp_conn_ctx* ctx = conn->context;
    assert(ctx != NULL);

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
        if((ctx->session_state == NO_SESSION && now - ctx->last_msg_time > SEC_US(MQTT_CONNECT_MAX_PENDING)) ||
            (ctx->session_state != NO_SESSION && now - ctx->last_msg_time > SEC_US(MQTT_TCP_MAX_IDLE)))
        {
            tmq_vec_push_back(timeout_conns, conn);
            tlog_info("connection timeout [%s]", (char*) (it.first));
        }
    }
    /* do remove after iteration to prevent iterator failure */
    tmq_tcp_conn_t** conn_it = tmq_vec_begin(timeout_conns);
    for(; conn_it != tmq_vec_end(timeout_conns); conn_it++)
        tmq_tcp_conn_close(get_ref(*conn_it));
    tmq_vec_free(timeout_conns);
}

static void handle_new_connection(void* arg)
{
    tmq_io_group_t* group = arg;

    pthread_mutex_lock(&group->pending_conns_lk);
    tmq_vec(tmq_socket_t) conns = tmq_vec_make(tmq_socket_t);
    tmq_vec_swap(&conns, &group->pending_conns);
    pthread_mutex_unlock(&group->pending_conns_lk);

    for(tmq_socket_t* it = tmq_vec_begin(conns); it != tmq_vec_end(conns); it++)
    {
        tmq_tcp_conn_t* conn = tmq_tcp_conn_new(group, *it, &group->broker->codec);
        conn->close_cb = remove_tcp_conn;
        conn->state = CONNECTED;

        tcp_conn_ctx* conn_ctx = malloc(sizeof(tcp_conn_ctx));
        tmq_vec_init(&conn_ctx->pending_packets, tmq_packet_t);
        conn_ctx->upstream.broker = group->broker;
        conn_ctx->session_state = NO_SESSION;
        conn_ctx->parsing_ctx.state = PARSING_FIXED_HEADER;
        conn_ctx->last_msg_time = time_now();
        tmq_tcp_conn_set_context(conn, conn_ctx);

        char conn_name[50];
        tmq_tcp_conn_id(conn, conn_name, sizeof(conn_name));
        tmq_map_put(group->tcp_conns, conn_name, get_ref(conn));
        assert(conn->ref_cnt == 1);

        tlog_info("new connection [%s] group=%p thread=%lu", conn_name, group, mqtt_tid);
    }
    tmq_vec_free(conns);
}

static void tmq_io_group_init(tmq_io_group_t* group, tmq_broker_t* broker)
{
    group->broker = broker;
    tmq_event_loop_init(&group->loop);
    tmq_map_str_init(&group->tcp_conns, tmq_tcp_conn_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);

    tmq_timer_t* timer = tmq_timer_new(SEC_MS(MQTT_TCP_CHECKALIVE_INTERVAL), 1, tcp_checkalive, group);
    group->tcp_checkalive_timer = tmq_event_loop_add_timer(&group->loop, timer);

    tmq_vec_init(&group->pending_conns, tmq_socket_t);
    tmq_notifier_init(&group->new_conn_notifier, &group->loop, handle_new_connection, group);
    pthread_mutex_init(&group->pending_conns_lk, NULL);
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

    tmq_notifier_destroy(&group->new_conn_notifier);
    tmq_event_loop_destroy(&group->loop);
    pthread_mutex_destroy(&group->pending_conns_lk);
}

static void tmq_io_group_run(tmq_io_group_t* group)
{
    if(pthread_create(&group->io_thread, NULL, io_group_thread_func, group) != 0)
        fatal_error("pthread_create() error %d: %s", errno, strerror(errno));
}

static void tmq_io_group_stop(tmq_io_group_t* group)
{
    tmq_event_loop_cancel_timer(&group->loop, group->tcp_checkalive_timer);
    tmq_event_loop_quit(&group->loop);
}

/* mqtt broker */

static void dispatch_new_connection(tmq_socket_t conn, void* arg)
{
    tmq_broker_t* broker = (tmq_broker_t*) arg;

    /* dispatch tcp connection using round-robin */
    tmq_io_group_t* next_group = &broker->io_groups[broker->next_io_group++];
    if(broker->next_io_group >= MQTT_IO_THREAD)
        broker->next_io_group = 0;

    pthread_mutex_lock(&next_group->pending_conns_lk);
    tmq_vec_push_back(next_group->pending_conns, conn);
    pthread_mutex_unlock(&next_group->pending_conns_lk);

    tmq_notifier_notify(&next_group->new_conn_notifier);
}

void handle_mqtt_connect(tmq_broker_t* broker, tmq_connect_pkt connect_pkt)
{
    tmq_connect_pkt_print(&connect_pkt);
}

void tmq_broker_init(tmq_broker_t* broker, const char* cfg)
{
    if(!broker) return;
    if(tmq_config_init(&broker->conf, cfg) == 0)
        tlog_info("read config file %s ok", cfg);
    else
    {
        tlog_error("read config file error");
        return;
    }

    tmq_event_loop_init(&broker->event_loop);
    tmq_codec_init(&broker->codec);

    tmq_str_t port_str = tmq_config_get(&broker->conf, "port");
    unsigned int port = port_str ? strtoul(port_str, NULL, 10): 1883;
    tlog_info("listening on port %u", port);
    tmq_acceptor_init(&broker->acceptor, &broker->event_loop, port);
    tmq_acceptor_set_cb(&broker->acceptor, dispatch_new_connection, broker);

    for(int i = 0; i < MQTT_IO_THREAD; i++)
        tmq_io_group_init(&broker->io_groups[i], broker);
    broker->next_io_group = 0;

    /* ignore SIGPIPE signal */
    signal(SIGPIPE, SIG_IGN);
}

void tmq_broker_run(tmq_broker_t* broker)
{
    if(!broker) return;
    for(int i = 0; i < MQTT_IO_THREAD; i++)
        tmq_io_group_run(&broker->io_groups[i]);
    tmq_acceptor_listen(&broker->acceptor);
    tmq_event_loop_run(&broker->event_loop);

    /* clean up */
    tmq_acceptor_destroy(&broker->acceptor);
    for(int i = 0; i < MQTT_IO_THREAD; i++)
    {
        tmq_io_group_stop(&broker->io_groups[i]);
        pthread_join(broker->io_groups[i].io_thread, NULL);
    }
    tmq_event_loop_destroy(&broker->event_loop);
}