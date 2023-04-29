//
// Created by zr on 23-4-18.
//

#ifndef TINYMQTT_MQTT_ACCEPTOR_H
#define TINYMQTT_MQTT_ACCEPTOR_H
#include "mqtt_socket.h"
#include "mqtt_event.h"

typedef void(*tmq_new_connection_cb)(tmq_socket_t conn, tmq_socket_addr_t* peer_addr, const void* arg);

typedef struct tmq_acceptor_s
{
    tmq_socket_t lis_socket;
    tmq_socket_t idle_socket;
    tmq_new_connection_cb connection_cb;
    const void* arg;
    int listening;
} tmq_acceptor_t;

void tmq_acceptor_init(tmq_acceptor_t* acceptor, tmq_event_loop_t* loop, uint16_t port);
void tmq_acceptor_set_cb(tmq_acceptor_t* acceptor, tmq_new_connection_cb cb, const void* arg);
void tmq_acceptor_listen(tmq_acceptor_t* acceptor);

#endif //TINYMQTT_MQTT_ACCEPTOR_H
