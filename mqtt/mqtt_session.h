//
// Created by zr on 23-4-20.
//

#ifndef TINYMQTT_MQTT_SESSION_H
#define TINYMQTT_MQTT_SESSION_H
#include "net/mqtt_tcp_conn.h"

typedef struct tmq_session_s
{
    tmq_tcp_conn_t* conn;
} tmq_session_t;

#endif //TINYMQTT_MQTT_SESSION_H
