//
// Created by zr on 23-4-15.
//

#ifndef TINYMQTT_MQTT_TCP_CONN_H
#define TINYMQTT_MQTT_TCP_CONN_H
#include "mqtt_socket.h"

typedef struct tmq_tcp_conn_s
{
    tmq_socket_t fd;

}tmq_tcp_conn_t;

#endif //TINYMQTT_MQTT_TCP_CONN_H
