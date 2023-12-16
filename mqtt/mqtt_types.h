//
// Created by zr on 23-6-2.
//

#ifndef TINYMQTT_MQTT_TYPES_H
#define TINYMQTT_MQTT_TYPES_H
#include "base/mqtt_vec.h"
#include "mqtt/mqtt_codec.h"

typedef tmq_vec(tmq_any_packet_t) packet_list;
typedef enum conn_state_e
{
    NO_SESSION,
    STARTING_SESSION,
    IN_SESSION
} conn_state_e;

typedef struct tmq_broker_s tmq_broker_t;
typedef struct tmq_client_s tiny_mqtt;

#define TCP_CONN_CTX_COMMON \
union                       \
{                           \
    tmq_broker_t* broker;   \
    tmq_session_t* session; \
    tiny_mqtt* client;      \
} upstream;                 \
conn_state_e conn_state;    \
pkt_parsing_ctx parsing_ctx;\

typedef struct tcp_conn_ctx_s
{
    TCP_CONN_CTX_COMMON
} tcp_conn_ctx;

typedef struct tcp_conn_broker_ctx_s
{
    TCP_CONN_CTX_COMMON
    packet_list pending_packets;
} tcp_conn_broker_ctx;

typedef struct session_connect_resp
{
    connack_return_code return_code;
    tmq_session_t* session;
    tmq_tcp_conn_t* conn;
    int session_present;
} session_connect_resp;

typedef struct tmq_message
{
    tmq_str_t message;
    uint8_t qos;
} tmq_message;

#endif //TINYMQTT_MQTT_TYPES_H
