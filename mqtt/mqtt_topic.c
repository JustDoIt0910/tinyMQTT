//
// Created by zr on 23-6-3.
//
#include "mqtt_topic.h"
#include "base/mqtt_util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static topic_tree_node* topic_tree_node_new(topic_tree_node* parent, char* level_name)
{
    topic_tree_node* node = malloc(sizeof(topic_tree_node));
    if(!node) fatal_error("malloc() error: out of memory");
    node->parent = parent;
    node->level_name = tmq_str_new(level_name);
    node->retain_message.message = NULL;
    tmq_map_str_init(&node->childs, topic_tree_node*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    /* parent == NULL means that this node is the root,
     * no subscribers will be added on it. */
    if(parent)
        tmq_map_str_init(&node->subscribers, uint8_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    return node;
}

static void topic_tree_node_free(topic_tree_node* node)
{
    tmq_map_free(node->childs);
    if(node->parent)
        tmq_map_free(node->subscribers);
    tmq_str_free(node->level_name);
    free(node);
}

void tmq_topics_init(tmq_topics_t* topics, tmq_broker_t* broker, match_cb on_match)
{
    if(!topics) return;
    topics->topic_tree_root = topic_tree_node_new(NULL, NULL);
    topics->sys_topic_tree_root = topic_tree_node_new(NULL, NULL);
    topics->on_match = on_match;
    topics->broker = broker;
}

static topic_tree_node* find_or_create(topic_tree_node* cur, tmq_str_t level, int create)
{
    topic_tree_node** next = tmq_map_get(cur->childs, level);
    if(next) return *next;
    if(!create) return NULL;
    topic_tree_node* new_next = topic_tree_node_new(cur, level);
    tmq_map_put(cur->childs, level, new_next);
    return new_next;
}

static topic_tree_node* add_topic_or_find(tmq_topics_t* topics, char* topic_filter, int add)
{
    char* lp = topic_filter, *rp = lp;
    topic_tree_node* node = topics->topic_tree_root;
    tmq_str_t level = tmq_str_empty();
    while(1)
    {
        while(*rp && *rp != '/') rp++;
        if(rp == lp)
        {
            level = tmq_str_assign(level, "");
            node = find_or_create(node, level, add);
            if(!node) break;
        }
        else
        {
            level = tmq_str_assign_n(level, lp, rp - lp);
            node = find_or_create(node, level, add);
            if(!node) break;
        }
        if(!*rp) break;
        lp = rp + 1; rp = lp;
        if(!*lp)
        {
            level = tmq_str_assign(level, "");
            node = find_or_create(node, level, add);
            break;
        }
    }
    tmq_str_free(level);
    return node;
}

void tmq_topics_add_subscription(tmq_topics_t* topics, char* topic_filter, char* client_id, uint8_t qos)
{
    if(!topic_filter || strlen(topic_filter) < 1)
        return;
    topic_tree_node* node = add_topic_or_find(topics, topic_filter, 1);
    assert(node != NULL);
    tmq_map_put(node->subscribers, client_id, qos);
}

static void try_remove_topic(tmq_topics_t* topics, topic_tree_node* node)
{
    /* when a topic has no subscribers, no sub-topics and no retained message, it can be removed */
    while(tmq_map_size(node->subscribers) == 0
          && tmq_map_size(node->childs) == 0
          && !node->retain_message.message)
    {
        topic_tree_node* parent = node->parent;
        tmq_map_erase(parent->childs, node->level_name);
        topic_tree_node_free(node);
        node = parent;
        if(node == topics->topic_tree_root || node == topics->sys_topic_tree_root)
            break;
    }
}

void tmq_topics_remove_subscription(tmq_topics_t* topics, char* topic_filter, char* client_id)
{
    topic_tree_node* node = add_topic_or_find(topics, topic_filter, 0);
    if(!node)
    {
        tlog_warn("topic filter doesn't exist: %s", topic_filter);
        return;
    }
    tmq_map_erase(node->subscribers, client_id);
    try_remove_topic(topics, node);
}

static void match(tmq_topics_t* topics, topic_tree_node* node, int n, int is_any_wildcard,
                  str_vec* levels, tmq_message* message, int retain)
{
    if(n == tmq_vec_size(*levels) || is_any_wildcard)
    {
        tmq_map_iter_t it = tmq_map_iter(node->subscribers);
        for(; tmq_map_has_next(it); tmq_map_next(node->subscribers, it))
        {
            char* client_id = it.first;
            uint8_t required_qos = *(uint8_t*) it.second;
            topics->on_match(topics->broker, client_id, required_qos, message);
        }
        if(n == tmq_vec_size(*levels))
        {
            /* if this is a retained message, save this message under the topic */
            if(retain)
            {
                node->retain_message.message = tmq_str_assign(node->retain_message.message, message->message);
                node->retain_message.qos = message->qos;
            }
            /* "#" includes the parent */
            topic_tree_node** next = tmq_map_get(node->childs, "#");
            if(next)
            {
                it = tmq_map_iter((*next)->subscribers);
                for(; tmq_map_has_next(it); tmq_map_next((*next)->subscribers, it))
                {
                    char* client_id = it.first;
                    uint8_t required_qos = *(uint8_t*) it.second;
                    topics->on_match(topics->broker, client_id, required_qos, message);
                }
            }
        }
        return;
    }
    topic_tree_node** next;
    char* level = *tmq_vec_at(*levels, n);
    if((next = tmq_map_get(node->childs, level)) != NULL)
        match(topics, *next, n + 1, 0, levels, message, retain);
    else if(retain)
    {
        topic_tree_node* new_next = topic_tree_node_new(node, level);
        tmq_map_put(node->childs, level, new_next);
        match(topics, new_next, n + 1, 0, levels, message, retain);
    }
    if((next = tmq_map_get(node->childs, "+")) != NULL)
        match(topics, *next, n + 1, 0, levels, message, 0);
    if((next = tmq_map_get(node->childs, "#")) != NULL)
        match(topics, *next, n + 1, 1, levels, message, 0);
}

void tmq_topics_publish(tmq_topics_t* topics, int sys, char* topic, tmq_message* message, int retain)
{
    char* lp = topic, *rp = lp;
    tmq_str_t level;
    str_vec levels = tmq_vec_make(tmq_str_t);
    while(1)
    {
        while(*rp && *rp != '/') rp++;
        if(rp == lp)
        {
            level = tmq_str_new("");
            tmq_vec_push_back(levels, level);
        }
        else
        {
            level = tmq_str_new_len(lp, rp - lp);
            tmq_vec_push_back(levels, level);
        }
        if(!*rp) break;
        lp = rp + 1; rp = lp;
        if(!*lp)
        {
            level = tmq_str_new("");
            tmq_vec_push_back(levels, level);
            break;
        }
    }
    topic_tree_node* root = sys ? topics->sys_topic_tree_root : topics->topic_tree_root;
    match(topics, root, 0, 0, &levels, message, retain);
    for(tmq_str_t* it = tmq_vec_begin(levels); it != tmq_vec_end(levels); it++)
        tmq_str_free(*it);
    tmq_vec_free(levels);
}

void tmq_topics_info(tmq_topics_t* topics)
{

}