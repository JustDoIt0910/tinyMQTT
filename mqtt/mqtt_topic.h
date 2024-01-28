//
// Created by zr on 23-6-3.
//

#ifndef TINYMQTT_MQTT_TOPIC_H
#define TINYMQTT_MQTT_TOPIC_H
#include "base/mqtt_str.h"
#include "base/mqtt_map.h"
#include "mqtt_types.h"

typedef struct retain_message_s
{
    mqtt_message retain_msg;
    tmq_str_t retain_topic;
} retain_message_t;
typedef tmq_vec(retain_message_t*) retain_message_list_t;

typedef struct subscribe_info_s
{
    tmq_session_t* session;
    uint8_t qos;
} subscribe_info_t;
typedef tmq_map(char*, subscribe_info_t) subscribe_map_t;

typedef struct member_sub_info_s
{
    struct member_sub_info_s* next;
    tmq_str_t node_addr;
} member_sub_info_t;

typedef struct route_table_item_s route_table_item_t;
typedef tmq_map(char*, struct topic_tree_node_s*) child_map;
typedef struct topic_tree_node_s
{
    tmq_str_t level_name;
    struct topic_tree_node_s* parent;
    /* next level */
    child_map children;
    /* the subscriber's client_id and max qos */
    subscribe_map_t* subscribers;
    /* cluster members that subscribe this topic */
    member_sub_info_t* subscribe_members;
    /* used to quickly delete route item in route table when removing
     * cluster members' subscription */
    route_table_item_t** route_table_item;
    /* store retain message */
    retain_message_t* retain_message;
} topic_tree_node_t;

typedef tmq_map(char*, int) member_addr_set;
typedef struct tmq_cluster_s tmq_cluster_t;
typedef void(*client_match_cb)(tmq_broker_t* broker, char* topic, mqtt_message* message, subscribe_map_t* subscribers);
typedef void(*route_match_cb)(tmq_cluster_t* cluster, char* topic, mqtt_message* message, member_addr_set* matched_members);
typedef struct tmq_topics_s
{
    topic_tree_node_t* root;
    client_match_cb client_on_match;
    route_match_cb route_on_match;
    tmq_broker_t* broker;
    member_addr_set matched_members;
} tmq_topics_t;

void tmq_topics_init(tmq_topics_t* topics, tmq_broker_t* broker, client_match_cb client_on_match,
                     route_match_cb route_on_match);
retain_message_list_t tmq_topics_add_subscription(tmq_topics_t* topics, char* topic_filter, tmq_session_t* session,
                                                  uint8_t qos, int* topic_exist, topic_tree_node_t** end_node);
void tmq_topics_add_route(tmq_topics_t* topics, char* topic_filter, char* member_addr, topic_tree_node_t** end_node);
void tmq_topics_remove_subscription(tmq_topics_t* topics, char* topic_filter, char* client_id);
void tmq_topics_publish(tmq_topics_t* topics, char* topic, mqtt_message* message, int retain, int is_tunneled);
void tmq_topic_split(char* str, str_vec* levels);
void tmq_topics_info(tmq_topics_t* topics);

#endif //TINYMQTT_MQTT_TOPIC_H
