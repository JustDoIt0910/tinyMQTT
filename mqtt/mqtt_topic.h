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
    tmq_message retain_msg;
    tmq_str_t retain_topic;
} retain_message_t;
typedef tmq_vec(retain_message_t*) retain_message_list_t;

typedef struct subscribe_info_s
{
    tmq_session_t* session;
    uint8_t qos;
} subscribe_info_t;
typedef tmq_map(char*, subscribe_info_t) subscribe_map_t;

typedef struct topic_tree_node
{
    tmq_str_t level_name;
    struct topic_tree_node* parent;
    /* next level */
    tmq_map(char*, struct topic_tree_node*) children;
    /* the subscriber's client_id and max qos */
    subscribe_map_t* subscribers;
    retain_message_t* retain_message;
} topic_tree_node;

typedef void(*match_cb)(tmq_broker_t* broker, char* topic, tmq_message* message,
        subscribe_map_t* subscribers);
typedef struct tmq_topics_s
{
    topic_tree_node* root;
    match_cb on_match;
    tmq_broker_t* broker;
} tmq_topics_t;

void tmq_topics_init(tmq_topics_t* topics, tmq_broker_t* broker, match_cb on_match);
retain_message_list_t tmq_topics_add_subscription(tmq_topics_t* topics, char* topic_filter,
                                                  tmq_session_t* session, uint8_t qos);
void tmq_topics_remove_subscription(tmq_topics_t* topics, char* topic_filter, char* client_id);
void tmq_topics_publish(tmq_topics_t* topics, char* topic, tmq_message* message, int retain);
void tmq_topics_info(tmq_topics_t* topics);

#endif //TINYMQTT_MQTT_TOPIC_H
