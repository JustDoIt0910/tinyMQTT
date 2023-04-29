//
// Created by zr on 23-4-15.
//

#ifndef TINYMQTT_MQTT_TCP_CONN_H
#define TINYMQTT_MQTT_TCP_CONN_H
#include "mqtt_socket.h"
#include "mqtt_buffer.h"
#include "mqtt_event.h"
#include <pthread.h>

typedef struct tmq_tcp_conn_s tmq_tcp_conn_t;
typedef void(*tcp_message_cb)(tmq_tcp_conn_t* conn, tmq_buffer_t* buffer);
typedef void(*tcp_close_cb)(tmq_tcp_conn_t* conn);
typedef void(*tcp_error_cb)(tmq_tcp_conn_t* conn);

typedef struct tmq_tcp_conn_s
{
    tmq_socket_t fd;
    tmq_socket_addr_t local_addr;
    tmq_socket_addr_t peer_addr;
    int ref_cnt;
    tmq_buffer_t in_buffer;
    tmq_buffer_t out_buffer;
    tmq_event_loop_t* loop;
    tmq_event_handler_t* read_event_handler;
    tmq_event_handler_t* write_event_handler;
    tmq_event_handler_t* error_close_handler;
    tcp_message_cb message_cb;
    tcp_close_cb close_cb;
    tcp_error_cb error_cb;
    tmq_destroy_cb destroy_cb;
    pthread_mutex_t lk;
} tmq_tcp_conn_t;

tmq_tcp_conn_t* tmq_tcp_conn_new(tmq_event_loop_t* loop, tmq_socket_t fd,
                                 tcp_message_cb message_cb, tcp_close_cb close_cb, tcp_error_cb error_cb);
int tmq_tcp_conn_id(tmq_tcp_conn_t* conn, char* buf, size_t buf_size);

#endif //TINYMQTT_MQTT_TCP_CONN_H
