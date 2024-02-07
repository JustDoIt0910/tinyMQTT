//
// Created by just do it on 2023/8/24.
//

#ifndef TINYMQTT_MQTT_CONTEXTS_H
#define TINYMQTT_MQTT_CONTEXTS_H
#include "mqtt_packet.h"
#include "mqtt_topic.h"
#include "base/mqtt_util.h"
#include "cluster/mqtt_cluster.h"
#include <stdlib.h>

/*************** session operations *****************/
typedef enum session_req_op_e
{
    SESSION_CONNECT,
    SESSION_DISCONNECT,
    SESSION_FORCE_CLOSE
} session_req_op;

typedef struct session_connect_req
{
    tmq_connect_pkt* connect_pkt;
    tmq_tcp_conn_t* conn;
} session_connect_req;

typedef struct session_req
{
    session_req_op op;
    union
    {
        session_connect_req connect_req;
        tmq_session_t* session;
    };
} session_req;

typedef struct session_operation_s
{
    tmq_broker_t* broker;
    session_req req;
} session_operation_t;

/*************** topic operations *****************/
typedef enum topic_req_op_e
{
    TOPIC_SUBSCRIBE,
    TOPIC_UNSUBSCRIBE
} topic_req_op;

typedef struct topic_req
{
    topic_req_op op;
    tmq_str_t client_id;
    union
    {
        tmq_subscribe_pkt subscribe_pkt;
        tmq_unsubscribe_pkt unsubscribe_pkt;
    };
} topic_req;

typedef struct topic_operation_s
{
    tmq_broker_t* broker;
    topic_req req;
} topic_operation_t;

/*************** publish task *****************/
typedef struct publish_req
{
    tmq_str_t topic;
    mqtt_message message;
    uint8_t retain;
    uint8_t is_tunneled_pub;
    tmq_str_t publisher_username;
    tmq_str_t publisher_client_id;
} publish_req;

typedef struct publish_ctx_s
{
    tmq_broker_t* broker;
    publish_req req;
} publish_ctx_t;

typedef struct broadcast_ctx_s
{
    tmq_str_t topic;
    mqtt_message message;
    int retain;
    tmq_vec(subscribe_info_t) subscribers;
} broadcast_ctx_t;

/*************** packet sending task *****************/
typedef struct packet_send_ctx_s
{
    tmq_tcp_conn_t* conn;
    tmq_any_packet_t pkt;
} packet_send_ctx_t;

/*************** cluster operations ****************/

typedef struct member_operation_s
{
    enum member_op_type {MEMBER_ADD, MEMBER_DEL} op;
    tmq_cluster_t* cluster;
    tmq_tcp_conn_t* member_conn;
} member_operation_t;

typedef struct member_route_operation_s
{
    enum route_op_type {ROUTE_ADD, ROUTE_DEL} op;
    tmq_cluster_t* cluster;
    tmq_str_t member_addr;
    tmq_str_t topic_filters;
} member_route_operation_t;

#endif //TINYMQTT_MQTT_CONTEXTS_H
