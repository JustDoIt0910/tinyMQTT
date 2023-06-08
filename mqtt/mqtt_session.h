//
// Created by zr on 23-4-20.
//

#ifndef TINYMQTT_MQTT_SESSION_H
#define TINYMQTT_MQTT_SESSION_H
#include "net/mqtt_tcp_conn.h"

typedef enum session_state_e{OPEN, CLOSED} session_state_e;

typedef struct tmq_session_s
{
    tmq_str_t client_id;
    tmq_tcp_conn_t* conn;
    session_state_e state;
    int clean_session;
    tmq_map(char*, uint8_t) subscriptions;
    void* upstream;
} tmq_session_t;

tmq_session_t* tmq_session_new(void* upstream, tmq_tcp_conn_t* conn, tmq_str_t client_id, int clean_session);
void tmq_session_send_packet(tmq_session_t* session, tmq_any_packet_t* pkt);

#endif //TINYMQTT_MQTT_SESSION_H
