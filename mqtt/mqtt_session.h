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
    topic_list topic_filters;
} tmq_session_t;

tmq_session_t* tmq_session_new(tmq_tcp_conn_t* conn, int clean_session);
void tmq_session_handle_subscribe(tmq_session_t* session, tmq_subscribe_pkt subscribe_pkt);

#endif //TINYMQTT_MQTT_SESSION_H
