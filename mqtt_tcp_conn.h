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
    tmq_codec_interface_t* message_codec;

    tmq_socket_addr_t local_addr;
    tmq_socket_addr_t peer_addr;

    tmq_buffer_t in_buffer;
    tmq_buffer_t out_buffer;

    tmq_event_handler_t* read_event_handler;
    tmq_event_handler_t* write_event_handler;
    tmq_event_handler_t* error_close_handler;

    tcp_close_cb close_cb;
    void* close_cb_arg;
} tmq_tcp_conn_t;

tmq_tcp_conn_t* tmq_tcp_conn_new(tmq_io_group_t* group, tmq_socket_t fd, tmq_codec_interface_t* message_codec);
void close_conn(tmq_tcp_conn_t* conn);
int tmq_tcp_conn_id(tmq_tcp_conn_t* conn, char* buf, size_t buf_size);

#endif //TINYMQTT_MQTT_TCP_CONN_H
