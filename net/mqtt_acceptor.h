//
// Created by zr on 23-4-18.
//

#ifndef TINYMQTT_MQTT_ACCEPTOR_H
#define TINYMQTT_MQTT_ACCEPTOR_H
#include "mqtt_socket.h"
#include "event/mqtt_event.h"

typedef void(*tmq_new_connection_cb)(tmq_socket_t conn, void* arg);

typedef struct tmq_acceptor_s
{
    tmq_event_loop_t* loop;
    tmq_socket_t lis_socket;
    int is_unix;
    tmq_socket_t idle_socket;
    tmq_event_handler_t* new_conn_handler;
    tmq_new_connection_cb connection_cb;
    void* arg;
    int listening;
} tmq_acceptor_t;

void tmq_acceptor_init(tmq_acceptor_t* acceptor, tmq_event_loop_t* loop, uint16_t port);
void tmq_unix_acceptor_init(tmq_acceptor_t* acceptor, tmq_event_loop_t* loop, const char* path);
void tmq_acceptor_set_cb(tmq_acceptor_t* acceptor, tmq_new_connection_cb cb, void* arg);
void tmq_acceptor_listen(tmq_acceptor_t* acceptor);
void tmq_acceptor_destroy(tmq_acceptor_t* acceptor);

#endif //TINYMQTT_MQTT_ACCEPTOR_H
