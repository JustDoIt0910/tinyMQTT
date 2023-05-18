//
// Created by zr on 23-4-9.
//
#include "mqtt_broker.h"
#include "mqtt_tcp_conn.h"
#include "mqtt_util.h"
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

static void remove_tcp_conn(tmq_tcp_conn_t* conn, void* arg)
{
    tmq_io_group_t* group = conn->group;
    active_ctx* ctx = tmq_map_get(group->tcp_conns, conn);
    assert(ctx != NULL);
    tmq_cancel_timer(ctx->timer);
    tmq_map_erase(group->tcp_conns, conn);

    char conn_name[50];
    tmq_tcp_conn_id(conn, conn_name, sizeof(conn_name));
    tlog_info("remove connection [%s]", conn_name);
}

static void handle_timeout(void* arg)
{
    tmq_tcp_conn_t* conn = (tmq_tcp_conn_t*) arg;
    tmq_io_group_t* group = conn->group;

    active_ctx* ctx = tmq_map_get(group->tcp_conns, conn);
    assert(ctx != NULL);
    ctx->ttl--;
    if(ctx->ttl <= 0)
    {
        char conn_name[50];
        tmq_tcp_conn_id(conn, conn_name, sizeof(conn_name));
        tlog_info("connection timeout [%s]", conn_name);
        tmq_tcp_conn_destroy(conn);
    }
}

static void handle_new_connection(int fd, uint32_t event, const void* arg)
{
    tmq_io_group_t* group = (tmq_io_group_t*) arg;
    int buf[128];
    ssize_t n = read(fd, buf, sizeof(buf));
    if(n < 0)
        fatal_error("read() error %d: %s", errno, strerror(errno));

    pthread_mutex_lock(&group->pending_conns_lk);
    tmq_vec(tmq_socket_t) conns = tmq_vec_make(tmq_socket_t);
    tmq_vec_swap(&conns, &group->pending_conns);
    pthread_mutex_unlock(&group->pending_conns_lk);

    for(tmq_socket_t* it = tmq_vec_begin(conns); it != tmq_vec_end(conns); it++)
    {
        tmq_tcp_conn_t* conn = tmq_tcp_conn_new(group, *it, &group->broker->codec);
        tcp_conn_ctx* conn_ctx = malloc(sizeof(tcp_conn_ctx));
        tmq_tcp_conn_set_context(conn, conn_ctx);
        conn->close_cb = remove_tcp_conn;

        tmq_timer_t* timer = tmq_timer_new(MQTT_ALIVE_TIMER_INTERVAL, 1, handle_timeout, conn);
        active_ctx ctx = {
            .timer = timer,
            .ttl = MQTT_CONNECT_PENDING
        };
        tmq_map_put(group->tcp_conns, conn, ctx);
        tmq_event_loop_add_timer(&group->loop, timer);

        char conn_name[50];
        tmq_tcp_conn_id(conn, conn_name, sizeof(conn_name));
        tlog_info("new connection [%s] group=%p thread=%lu", conn_name, group, mqtt_tid);
    }
    tmq_vec_free(conns);
}

static void tmq_io_group_init(tmq_io_group_t* group, tmq_broker_t* broker)
{
    group->broker = broker;
    tmq_event_loop_init(&group->loop);
    tmq_map_64_init(&group->tcp_conns, active_ctx, MAP_DEFAULT_CAP,MAP_DEFAULT_LOAD_FACTOR);
    tmq_vec_init(&group->pending_conns, tmq_socket_t);
    pthread_mutex_init(&group->pending_conns_lk, NULL);

    if(pipe2(group->wakeup_pipe, O_CLOEXEC | O_NONBLOCK) < 0)
        fatal_error("pipe2() error %d: %s", errno, strerror(errno));

    group->wakeup_handler = tmq_event_handler_create(group->wakeup_pipe[0], EPOLLIN,
                                                     handle_new_connection, group);
    tmq_event_loop_register(&group->loop, group->wakeup_handler);
}

static void* io_group_thread_func(void* arg)
{
    tmq_io_group_t* group = (tmq_io_group_t*) arg;
    tmq_event_loop_run(&group->loop);
    tmq_event_loop_clean(&group->loop);
}

static void tmq_io_group_run(tmq_io_group_t* group)
{
    if(pthread_create(&group->io_thread, NULL, io_group_thread_func, group) != 0)
        fatal_error("pthread_create() error %d: %s", errno, strerror(errno));
}

static void dispatch_new_connection(tmq_socket_t conn, void* arg)
{
    tmq_broker_t* broker = (tmq_broker_t*) arg;
    tmq_io_group_t* next_group = &broker->io_groups[broker->next_io_group++];
    if(broker->next_io_group >= MQTT_IO_THREAD)
        broker->next_io_group = 0;

    pthread_mutex_lock(&next_group->pending_conns_lk);
    tmq_vec_push_back(next_group->pending_conns, conn);
    pthread_mutex_unlock(&next_group->pending_conns_lk);

    int wakeup = 1;
    write(next_group->wakeup_pipe[1], &wakeup, sizeof(wakeup));
}

void tmq_broker_init(tmq_broker_t* broker, uint16_t port)
{
    if(!broker) return;
    tmq_event_loop_init(&broker->event_loop);

    tmq_acceptor_init(&broker->acceptor, &broker->event_loop, port);
    tmq_acceptor_set_cb(&broker->acceptor, dispatch_new_connection, broker);

    tmq_codec_init(&broker->codec);

    for(int i = 0; i < MQTT_IO_THREAD; i++)
        tmq_io_group_init(&broker->io_groups[i], broker);
    broker->next_io_group = 0;
}

void tmq_broker_run(tmq_broker_t* broker)
{
    if(!broker) return;
    for(int i = 0; i < MQTT_IO_THREAD; i++)
        tmq_io_group_run(&broker->io_groups[i]);
    tmq_acceptor_listen(&broker->acceptor);
    tmq_event_loop_run(&broker->event_loop);

    tmq_event_loop_clean(&broker->event_loop);

}