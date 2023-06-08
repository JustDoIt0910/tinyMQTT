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
typedef struct tcp_conn_ctx_s
{
    int64_t last_msg_time;
    union
    {
        tmq_broker_t* broker;
        tmq_session_t* session;
    } upstream;
    conn_state_e conn_state;
    pkt_parsing_ctx parsing_ctx;
    packet_list pending_packets;
} tcp_conn_ctx;

typedef struct session_connect_req
{
    tmq_connect_pkt connect_pkt;
    tmq_tcp_conn_t* conn;
} session_connect_req;

typedef struct session_connect_resp
{
    connack_return_code return_code;
    tmq_session_t* session;
    tmq_tcp_conn_t* conn;
    int session_present;
} session_connect_resp;
typedef tmq_vec(session_connect_resp) connect_resp_list;

typedef enum session_ctl_op_e
{
    SESSION_CONNECT,
    SESSION_DISCONNECT,
    SESSION_CLOSE
} session_ctl_op;

typedef struct session_ctl
{
    session_ctl_op op;
    union
    {
        session_connect_req start_req;
        tmq_session_t* session;
    } context;
} session_ctl;
typedef tmq_vec(session_ctl) session_ctl_list;

typedef struct tmq_message
{
    tmq_str_t message;
    uint8_t qos;
} tmq_message;
typedef tmq_vec(tmq_message) message_list;
typedef tmq_vec(tmq_message*) message_ptr_list;

typedef struct subscribe_req
{
    tmq_str_t client_id;
    topic_list topic_filters;
} subscribe_req;

typedef struct unsubscribe_req
{

} unsubscribe_req;

typedef struct publish_req
{

} publish_req;

typedef enum message_ctl_op_e
{
    SUBSCRIBE,
    UNSUBSCRIBE,
    PUBLISH
} message_ctl_op;

typedef struct message_ctl
{
    message_ctl_op op;
    union
    {
        subscribe_req sub_req;
        unsubscribe_req unsub_req;
        publish_req pub_req;
    } context;
} message_ctl;
typedef tmq_vec(message_ctl) message_ctl_list;

typedef struct packet_send_req
{
    tmq_tcp_conn_t* conn;
    tmq_any_packet_t pkt;
} packet_send_req;
typedef tmq_vec(packet_send_req) packet_send_list;

#endif //TINYMQTT_MQTT_TYPES_H
