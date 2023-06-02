//
// Created by zr on 23-4-20.
//

#ifndef TINYMQTT_MQTT_SESSION_H
#define TINYMQTT_MQTT_SESSION_H
#include "net/mqtt_tcp_conn.h"

typedef enum session_state_e{OPEN, CLOSED} session_state_e;

typedef struct tmq_session_s
{
    tmq_tcp_conn_t* conn;
    session_state_e state;
    int clean_session;
} tmq_session_t;

tmq_session_t* tmq_session_new(int clean_session);

#endif //TINYMQTT_MQTT_SESSION_H
