//
// Created by zr on 23-6-2.
//

#ifndef TINYMQTT_MQTT_TYPES_H
#define TINYMQTT_MQTT_TYPES_H
#include "base/mqtt_vec.h"
#include "codec/mqtt_proto_codec.h"
#include "codec/mqtt_console_codec.h"

typedef enum conn_state_e
{
    NO_SESSION,
    STARTING_SESSION,
    IN_SESSION
} conn_state_e;

typedef struct tmq_broker_s tmq_broker_t;
typedef struct tmq_client_s tiny_mqtt;

typedef struct tcp_conn_ctx_s
{
    union
    {
        tmq_broker_t* broker;
        tmq_session_t* session;
        tiny_mqtt* client;
    } upstream;
    conn_state_e conn_state;
    mqtt_parsing_ctx_t parsing_ctx;
} tcp_conn_ctx_t;

typedef struct console_conn_ctx_s
{
    tmq_broker_t* broker;
    console_parsing_ctx_t parsing_ctx;
} console_conn_ctx_t;

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
