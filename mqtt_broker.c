//
// Created by zr on 23-4-9.
//
#include "mqtt_broker.h"
#include <assert.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <string.h>
#include "tlog.h"

static void pending_connection_timeout(void* arg)
{
    tlog_info("mqtt connection timeout");
    tmq_tcp_conn_t* conn = (tmq_tcp_conn_t*) arg;
    tmq_tcp_conn_close(getRef(conn));
    releaseRef(conn);
}

static void on_tcp_conn_close(tmq_tcp_conn_t* conn, void* arg)
{
    tlog_info("tcp connection closed before starting mqtt session");
    tmq_broker_t* broker = (tmq_broker_t*) arg;
    pthread_mutex_lock(&broker->tcp_conns_lk);
    tmq_timer_t** timer = tmq_map_get(broker->pending_tcp_conns, conn);
    if(timer)
    {
        tmq_map_erase(broker->pending_tcp_conns, conn);
        releaseRef(conn);
    }
    pthread_mutex_unlock(&broker->tcp_conns_lk);
    releaseRef(conn);
}

static void on_new_connection(tmq_socket_t fd, void* arg)
{
    assert(arg != NULL);
    tmq_broker_t* broker = (tmq_broker_t*) arg;
    tmq_tcp_conn_t* conn = tmq_tcp_conn_new(&broker->event_loop, fd, (tmq_codec_interface_t*) &broker->codec);
    tmq_tcp_conn_set_close_cb(getRef(conn), on_tcp_conn_close, broker);

    tmq_timer_t* timer = tmq_timer_new(MQTT_CONNECT_PENDING, 0, pending_connection_timeout, getRef(conn));
    tmq_map_put(broker->pending_tcp_conns, getRef(conn), timer);
    assert(conn->ref_cnt == 3);

    tmq_event_loop_add_timer(&broker->event_loop, timer);
    tmq_tcp_conn_start(getRef(conn));

    releaseRef(conn);
}

static void on_connect_pkt(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_connect_pkt pkt)
{

}

void tmq_broker_init(tmq_broker_t* broker, uint16_t port)
{
    if(!broker) return;
    tmq_event_loop_init(&broker->event_loop);

    tmq_acceptor_init(&broker->acceptor, &broker->event_loop, port);
    tmq_acceptor_set_cb(&broker->acceptor, on_new_connection, broker);

    pthread_mutex_init(&broker->tcp_conns_lk, NULL);
    tmq_map_64_init(&broker->pending_tcp_conns, tmq_timer_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);

    connect_msg_codec_init(&broker->codec, broker, on_connect_pkt);
}

void tmq_broker_run(tmq_broker_t* broker)
{
    if(!broker) return;
    tmq_event_loop_run(&broker->event_loop);
    tmq_event_loop_clean(&broker->event_loop);
}