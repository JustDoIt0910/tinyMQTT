//
// Created by zr on 23-4-15.
//

#ifndef TINYMQTT_MQTT_TCP_CONN_H
#define TINYMQTT_MQTT_TCP_CONN_H
#include "mqtt_socket.h"
#include "mqtt_buffer.h"
#include <pthread.h>

typedef struct tmq_tcp_conn_s tmq_tcp_conn_t;
typedef void(*tcp_message_cb)(tmq_tcp_conn_t* conn, tmq_buffer_t* buffer);

typedef struct tmq_tcp_conn_s
{
    tmq_socket_t fd;
    tmq_buffer_t in_buffer;
    tmq_buffer_t out_buffer;
    tcp_message_cb message_cb;
    pthread_mutex_t lk;
} tmq_tcp_conn_t;

#endif //TINYMQTT_MQTT_TCP_CONN_H
