//
// Created by zr on 23-6-3.
//
#include "mqtt_topic.h"
#include "base/mqtt_util.h"
#include <stdlib.h>

static topic_tree_node* topic_tree_node_new(topic_tree_node* parent)
{
    topic_tree_node* node = malloc(sizeof(topic_tree_node));
    if(!node) fatal_error("malloc() error: out of memory");
    node->parent = parent;
    tmq_map_str_init(&node->childs, topic_tree_node*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    /* parent == NULL means that this node is the root,
     * no subscribers will be added on it. */
    if(parent)
        tmq_map_str_init(&node->subscribers, uint8_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    return node;
}

void tmq_topics_init(tmq_topics_t* topics, match_cb on_match)
{
    if(!topics) return;
    topics->topic_tree_root = topic_tree_node_new(NULL);
    topics->sys_topic_tree_root = topic_tree_node_new(NULL);
    topics->on_match = on_match;
}

void tmq_topics_add_subscription(tmq_topics_t* topics, char* topic_filter, char* client_id, uint8_t qos)
{

}

void tmq_topics_remove_subscription(tmq_topics_t* topics, char* topic_filter, char* client_id)
{

}

void tmq_topics_match(tmq_topics_t* topics, char* topic, tmq_message* message)
{

}