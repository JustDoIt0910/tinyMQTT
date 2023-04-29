//
// Created by zr on 23-4-9.
//
#include "mqtt_broker.h"

void new_connnection_cb(tmq_socket_t conn, tmq_socket_addr_t* peer_addr, const void* arg)
{

}

void tmq_broker_init(tmq_broker_t* broker, uint16_t port)
{
    if(!broker) return;
    tmq_event_loop_init(&broker->event_loop);
    tmq_acceptor_init(&broker->acceptor, &broker->event_loop, port);
    tmq_acceptor_set_cb(&broker->acceptor, new_connnection_cb, broker);
    pthread_mutex_init(&broker->conn_map_lk, NULL);
    tmq_map_str_init(&broker->conn_map, tmq_tcp_conn_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
}

void tmq_broker_run(tmq_broker_t* broker)
{
    if(!broker) return;
    tmq_event_loop_run(&broker->event_loop);
    tmq_event_loop_clean(&broker->event_loop);
}