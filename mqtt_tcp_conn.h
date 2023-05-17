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

typedef struct tmq_tcp_conn_s tmq_tcp_conn_t;
typedef void(*tcp_close_cb)(tmq_tcp_conn_t* conn, void* arg);
typedef void(*tcp_error_cb)(tmq_tcp_conn_t* conn, void* arg);

enum conn_state_e {CONNECTING, CONNECTED, DISCONNECTING, DISCONNECTED};

typedef struct tmq_tcp_conn_s
{
    tmq_socket_t fd;
    enum conn_state_e state;
    int ref_cnt;
    tmq_event_loop_t* loop;
    tmq_codec_interface_t* message_codec;
    pthread_mutex_t lk;

    tmq_socket_addr_t local_addr;
    tmq_socket_addr_t peer_addr;

    tmq_buffer_t in_buffer;
    tmq_buffer_t out_buffer;

    tmq_event_handler_t* read_event_handler;
    tmq_event_handler_t* write_event_handler;
    tmq_event_handler_t* error_close_handler;

    tcp_close_cb close_cb;
    tcp_error_cb error_cb;
    void* close_cb_arg;
    void* error_cb_arg;
    tmq_destroy_cb destroy_cb;
} tmq_tcp_conn_t;

tmq_tcp_conn_t* tmq_tcp_conn_new(tmq_event_loop_t* loop, tmq_socket_t fd, tmq_codec_interface_t* message_codec);
void tmq_tcp_conn_close(tmq_tcp_conn_t* conn);
void tmq_tcp_conn_start(tmq_tcp_conn_t* conn);
void tmq_tcp_conn_set_close_cb(tmq_tcp_conn_t* conn, tcp_close_cb cb, void* arg);
void tmq_tcp_conn_set_error_cb(tmq_tcp_conn_t* conn, tcp_error_cb cb, void* arg);
int tmq_tcp_conn_id(tmq_tcp_conn_t* conn, char* buf, size_t buf_size);

#endif //TINYMQTT_MQTT_TCP_CONN_H
