//
// Created by zr on 23-6-2.
//

#ifndef TINYMQTT_MQTT_DEF_H
#define TINYMQTT_MQTT_DEF_H
#include "base/mqtt_vec.h"
#include "mqtt/mqtt_codec.h"

#define MQTT_TCP_CHECKALIVE_INTERVAL    10
#define MQTT_CONNECT_MAX_PENDING        100
#define MQTT_TCP_MAX_IDLE               300
#define MQTT_IO_THREAD                  4

typedef tmq_vec(tmq_packet_t) pending_packet_list;
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
    pending_packet_list pending_packets;
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
} session_ctl_op_e;

typedef struct session_ctl
{
    session_ctl_op_e op;
    union
    {
        session_connect_req start_req;
        tmq_session_t* session;
    } context;
} session_ctl;
typedef tmq_vec(session_ctl) session_ctl_list;

#endif //TINYMQTT_MQTT_DEF_H
