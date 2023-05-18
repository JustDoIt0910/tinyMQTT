//
// Created by zr on 23-4-15.
//

#ifndef TINYMQTT_MQTT_TCP_CONN_H
#define TINYMQTT_MQTT_TCP_CONN_H
#include "mqtt_socket.h"
#include "mqtt_buffer.h"
#include "mqtt_event.h"
#include "mqtt_codec.h"
#include <pthread.h>

typedef struct tmq_io_group_s tmq_io_group_t;
typedef struct tmq_tcp_conn_s tmq_tcp_conn_t;
typedef void(*tcp_close_cb)(tmq_tcp_conn_t* conn, void* arg);

typedef struct tmq_tcp_conn_s
{
    tmq_socket_t fd;
    tmq_io_group_t* group;
    tmq_codec_t* codec;

    tmq_socket_addr_t local_addr, peer_addr;
    tmq_buffer_t in_buffer, out_buffer;

    tmq_event_handler_t* read_event_handler,
    *write_event_handler, *error_close_handler;

    tcp_close_cb close_cb;
    void* close_cb_arg;

    void* context;
} tmq_tcp_conn_t;

tmq_tcp_conn_t* tmq_tcp_conn_new(tmq_io_group_t* group, tmq_socket_t fd, tmq_codec_t* codec);
void tmq_tcp_conn_destroy(tmq_tcp_conn_t* conn);
int tmq_tcp_conn_id(tmq_tcp_conn_t* conn, char* buf, size_t buf_size);
void tmq_tcp_conn_set_context(tmq_tcp_conn_t* conn, void* ctx);

#endif //TINYMQTT_MQTT_TCP_CONN_H
