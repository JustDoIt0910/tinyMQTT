//
// Created by zr on 23-6-3.
//

#ifndef TINYMQTT_MQTT_TOPIC_H
#define TINYMQTT_MQTT_TOPIC_H
#include "base/mqtt_str.h"
#include "base/mqtt_map.h"
#include "mqtt_types.h"

typedef struct topic_tree_node
{
    tmq_str_t level_name;
    struct topic_tree_node* parent;
    /* next level */
    tmq_map(char*, struct topic_tree_node*) childs;
    /* the subscriber's client_id and max qos */
    tmq_map(char*, uint8_t) subscribers;
    tmq_message retain_message;
} topic_tree_node;

typedef void(*match_cb)(tmq_broker_t* broker, char* client_id,
                        char* topic, uint8_t required_qos, tmq_message* message);
typedef struct tmq_topics_s
{
    topic_tree_node* topic_tree_root;
    /* system topics */
    topic_tree_node* sys_topic_tree_root;
    match_cb on_match;
    tmq_broker_t* broker;
} tmq_topics_t;

void tmq_topics_init(tmq_topics_t* topics, tmq_broker_t* broker, match_cb on_match);
message_ptr_list tmq_topics_add_subscription(tmq_topics_t* topics, char* topic_filter, char* client_id, uint8_t qos);
void tmq_topics_remove_subscription(tmq_topics_t* topics, char* topic_filter, char* client_id);
void tmq_topics_publish(tmq_topics_t* topics, int sys, char* topic, tmq_message* message, int retain);
void tmq_topics_info(tmq_topics_t* topics);

#endif //TINYMQTT_MQTT_TOPIC_H
