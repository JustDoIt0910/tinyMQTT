//
// Created by zr on 23-4-9.
//
#include "mqtt_broker.h"

static void new_connnection_cb(tmq_socket_t fd, void* arg)
{
    tmq_broker_t* broker = (tmq_broker_t*) arg;
    tmq_tcp_conn_t* conn = tmq_tcp_conn_new(&broker->event_loop, fd, (tmq_codec_interface_t*) &broker->codec);
}

static void on_connect_pkt(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_connect_pkt pkt)
{

}

void tmq_broker_init(tmq_broker_t* broker, uint16_t port)
{
    if(!broker) return;
    tmq_event_loop_init(&broker->event_loop);

    tmq_acceptor_init(&broker->acceptor, &broker->event_loop, port);
    tmq_acceptor_set_cb(&broker->acceptor, new_connnection_cb, broker);

    pthread_mutex_init(&broker->conn_lk, NULL);

    tmq_map_64_init(&broker->pending_conns, tmq_tcp_conn_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);

    connect_msg_codec_init(&broker->codec, broker, on_connect_pkt);
}

void tmq_broker_run(tmq_broker_t* broker)
{
    if(!broker) return;
    tmq_event_loop_run(&broker->event_loop);
    tmq_event_loop_clean(&broker->event_loop);
}