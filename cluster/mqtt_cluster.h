//
// Created by just do it on 2024/1/23.
//

#ifndef TINYMQTT_MQTT_CLUSTER_H
#define TINYMQTT_MQTT_CLUSTER_H
#include "base/mqtt_map.h"
#include "mqtt_discovery.h"
#include "net/mqtt_acceptor.h"
#include "mqtt/mqtt_io_context.h"
#include "codec/mqtt_cluster_codec.h"

typedef struct tmq_broker_s tmq_broker_t;
typedef struct tmq_tcp_conn_s tmq_tcp_conn_t;
typedef struct topic_tree_node_s topic_tree_node_t;
typedef tmq_map(char*, tmq_tcp_conn_t*) member_map;

typedef struct route_table_item_s
{
    struct route_table_item_s* next;
    topic_tree_node_t* pos_in_topic_tree;
    char topic_filter_name[];
} route_table_item_t;
typedef tmq_map(char*, route_table_item_t*) route_table_t;

typedef struct tmq_cluster_s
{
    tmq_broker_t* broker;
    tmq_redis_discovery_t discovery;
    route_table_t route_table;
    member_map members;
    tmq_acceptor_t acceptor;
    tmq_cluster_codec_t codec;
    tmq_mailbox_t new_conn_box;
    tmq_mailbox_t message_send_box;
} tmq_cluster_t;

void tmq_cluster_init(tmq_broker_t* broker, tmq_cluster_t* node, const char* redis_ip, uint16_t redis_port,
                           const char* node_ip, uint16_t node_port);
void tmq_cluster_add_route(tmq_cluster_t* node, const char* topic_filter, topic_tree_node_t* topic_node);

typedef enum cluster_message_type_e
{
    CLUSTER_ROUTE_SYNC,
    CLUSTER_ROUTE_DEL,
    CLUSTER_PUBLISH
} cluster_message_type;

#endif //TINYMQTT_MQTT_CLUSTER_H
