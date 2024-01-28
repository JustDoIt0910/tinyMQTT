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
    IN_SESSION,
    TUNNELED
} conn_state_e;

typedef struct tmq_broker_s tmq_broker_t;
typedef struct tmq_client_s tiny_mqtt;

typedef struct tcp_conn_mqtt_ctx_s
{
    union
    {
        tmq_broker_t* broker;
        tmq_session_t* session;
        tiny_mqtt* client;
    } upstream;
    conn_state_e conn_state;
    mqtt_parsing_ctx_t parsing_ctx;
} tcp_conn_mqtt_ctx_t;

typedef struct tcp_conn_simple_ctx_s
{
    tmq_broker_t* broker;
    len_based_parsing_ctx_t parsing_ctx;
} tcp_conn_simple_ctx_t;

typedef struct session_connect_resp
{
    connack_return_code return_code;
    tmq_session_t* session;
    tmq_tcp_conn_t* conn;
    int session_present;
} session_connect_resp;

typedef struct mqtt_message
{
    tmq_str_t message;
    uint8_t qos;
} mqtt_message;

typedef enum user_op_type_e {ADD, DEL, MOD} user_op_type;
typedef struct user_op_context_s
{
    user_op_type op;
    tmq_broker_t* broker;
    tmq_tcp_conn_t* conn;
    tmq_str_t username;
    tmq_str_t password;
    int success;
    tmq_str_t reason;
} user_op_context_t;

#endif //TINYMQTT_MQTT_TYPES_H
