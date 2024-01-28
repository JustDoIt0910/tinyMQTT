//
// Created by zr on 23-6-3.
//
#include "mqtt_topic.h"
#include "mqtt_broker.h"
#include "mqtt_session.h"
#include "base/mqtt_util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

typedef tmq_vec(topic_tree_node_t*) topic_path;

/**
 * @brief   Split a topic string into levels. strtok()/strtok_r() is not suitable here because they
 *          ignore separators at the beginning and ending of the string. e.g. topic "a/b" and topic
 *          "/a/b" are different topics, "a/b" should be split into levels ["a", "b"], "/a/b" should
 *          be split into levels ["", "a", "b"].
 * @param str topic string
 * @param levels level names
 * */
void tmq_topic_split(char* str, str_vec* levels)
{
    tmq_str_t level;
    char* lp = str, *rp = lp;
    while(1)
    {
        while(*rp && *rp != '/') rp++;
        if(rp == lp)
        {
            level = tmq_str_new("");
            tmq_vec_push_back(*levels, level);
        }
        else
        {
            level = tmq_str_new_len(lp, rp - lp);
            tmq_vec_push_back(*levels, level);
        }
        if(!*rp) break;
        lp = rp + 1; rp = lp;
        if(!*lp)
        {
            level = tmq_str_new("");
            tmq_vec_push_back(*levels, level);
            break;
        }
    }
}

static topic_tree_node_t* topic_tree_node_new(topic_tree_node_t* parent, char* level_name)
{
    topic_tree_node_t* node = malloc(sizeof(topic_tree_node_t));
    if(!node) fatal_error("malloc() error: out of memory");
    bzero(node, sizeof(topic_tree_node_t));
    node->parent = parent;
    node->level_name = tmq_str_new(level_name);
    tmq_map_str_init(&node->children, topic_tree_node_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    return node;
}

static void topic_tree_node_t_free(topic_tree_node_t* node)
{
    tmq_map_free(node->children);
    if(node->subscribers)
    {
        tmq_map_free(*node->subscribers);
        free(node->subscribers);
    }
    tmq_str_free(node->level_name);
    free(node);
}

void tmq_topics_init(tmq_topics_t* topics, tmq_broker_t* broker, client_match_cb client_on_match,
                     route_match_cb route_on_match)
{
    if(!topics) return;
    topics->root = topic_tree_node_new(NULL, NULL);
    topics->client_on_match = client_on_match;
    topics->route_on_match = route_on_match;
    topics->broker = broker;
    tmq_map_str_init(&topics->matched_members, int, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
}

static topic_tree_node_t* find_or_create(topic_tree_node_t* cur, tmq_str_t level, int create)
{
    topic_tree_node_t** next = tmq_map_get(cur->children, level);
    if(next) return *next;
    if(!create) return NULL;
    topic_tree_node_t* new_next = topic_tree_node_new(cur, level);
    tmq_map_put(cur->children, level, new_next);
    return new_next;
}

static topic_tree_node_t* add_topic_or_find(tmq_topics_t* topics, char* topic_filter, int add, topic_path* path)
{
    char* lp = topic_filter, *rp = lp;
    topic_tree_node_t* node = topics->root;
    if(path) tmq_vec_push_back(*path, node);
    tmq_str_t level = tmq_str_empty();
    while(1)
    {
        while(*rp && *rp != '/') rp++;
        if(rp == lp)
        {
            level = tmq_str_assign(level, "");
            node = find_or_create(node, level, add);
            if(!node) break;
            if(path) tmq_vec_push_back(*path, node);
        }
        else
        {
            level = tmq_str_assign_n(level, lp, rp - lp);
            node = find_or_create(node, level, add);
            if(!node) break;
            if(path) tmq_vec_push_back(*path, node);
        }
        if(!*rp) break;
        lp = rp + 1; rp = lp;
        if(!*lp)
        {
            level = tmq_str_assign(level, "");
            node = find_or_create(node, level, add);
            if(path) tmq_vec_push_back(*path, node);
            break;
        }
    }
    tmq_str_free(level);
    return node;
}

/* retrieve all retained messages from the sub-topic */
static void get_all_retain(topic_tree_node_t* node, retain_message_list_t* retain_msg)
{
    if(node->retain_message)
        tmq_vec_push_back(*retain_msg, node->retain_message);
    tmq_map_iter_t it = tmq_map_iter(node->children);
    for(; tmq_map_has_next(it); tmq_map_next(node->children, it))
    {
        char* next_level = it.first;
        if(!strcmp(next_level, "+") || !strcmp(next_level, "#"))
            continue;
        topic_tree_node_t* next = *(topic_tree_node_t**) it.second;
        get_all_retain(next, retain_msg);
    }
}

static void find_retain_messages(topic_tree_node_t* cur, size_t next_idx, topic_path* path, retain_message_list_t* retain_msg)
{
    if(next_idx == tmq_vec_size(*path))
    {
        if(cur->retain_message)
            tmq_vec_push_back(*retain_msg, cur->retain_message);
        return;
    }
    topic_tree_node_t* next_node = *tmq_vec_at(*path, next_idx);
    /* if the subscription path contains single-level wildcards,
     * find retained messages in all matching sub-topics */
    if(!strcmp(next_node->level_name, "+"))
    {
        tmq_map_iter_t it = tmq_map_iter(cur->children);
        for(; tmq_map_has_next(it); tmq_map_next(cur->children, it))
        {
            char* next_level = it.first;
            if(!strcmp(next_level, "+") || !strcmp(next_level, "#"))
                continue;
            topic_tree_node_t* next = *(topic_tree_node_t**) it.second;
            find_retain_messages(next, next_idx + 1, path, retain_msg);
        }
    }
    else if(!strcmp(next_node->level_name, "#"))
        get_all_retain(cur, retain_msg);
    else
    {
        topic_tree_node_t** next = tmq_map_get(cur->children, next_node->level_name);
        if(next) find_retain_messages(*next, next_idx + 1, path, retain_msg);
    }
}

/**
 * @brief Add a new subscription to the topic tree
 *
 * @param topics the topic tree
 * @param topic_filter topic_filter of the subscription
 * @param session mqtt session of this subscription
 * @param qos subscribe qos
 * @param topic_exist[out] indicates if this topic is already exist in the topic tree
 * @param end_node[out] the leaf node corresponding to this topic
 *
 * @return an array of retained messages that matched this topic
 * */
retain_message_list_t tmq_topics_add_subscription(tmq_topics_t* topics, char* topic_filter, tmq_session_t* session,
                                                  uint8_t qos, int* topic_exist, topic_tree_node_t** end_node)
{
    retain_message_list_t retain_messages = tmq_vec_make(retain_message_t*);
    if(!topic_filter || strlen(topic_filter) < 1)
        return retain_messages;
    topic_path path = tmq_vec_make(topic_tree_node_t*);
    topic_tree_node_t* node = add_topic_or_find(topics, topic_filter, 1, &path);
    assert(node != NULL);
    if(!node->subscribers)
    {
        *topic_exist = 0;
        node->subscribers = malloc(sizeof(subscribe_map_t));
        tmq_map_str_init(node->subscribers, subscribe_info_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    }
    else
        *topic_exist = 1;
    subscribe_info_t info = {.session = session, .qos = qos};
    tmq_map_put(*node->subscribers, session->client_id, info);

    /* find all retained messages that matches this subscription */
    size_t i = 0; topic_tree_node_t* next;
    do
    {
        next = *tmq_vec_at(path, i + 1);
        if(strcmp(next->level_name, "+") != 0 && strcmp(next->level_name, "#") != 0)
            i++;
        else break;
    } while(i < tmq_vec_size(path) - 1);
    if(i == tmq_vec_size(path) - 1 && (*tmq_vec_at(path, i))->retain_message)
        tmq_vec_push_back(retain_messages, (*tmq_vec_at(path, i))->retain_message);
    else
        find_retain_messages(*tmq_vec_at(path, i), i + 1, &path, &retain_messages);
    tmq_vec_free(path);
    *end_node = node;
    return retain_messages;
}

void tmq_topics_add_route(tmq_topics_t* topics, char* topic_filter, char* member_addr, topic_tree_node_t** end_node)
{
    topic_tree_node_t* node = add_topic_or_find(topics, topic_filter, 1, NULL);
    assert(node != NULL);
    member_sub_info_t* member_sub = malloc(sizeof(member_sub_info_t));
    member_sub->node_addr = tmq_str_new(member_addr);
    member_sub->next = node->subscribe_members;
    node->subscribe_members = member_sub;
    *end_node = node;
}

static void try_remove_topic(tmq_topics_t* topics, topic_tree_node_t* node)
{
    /* when a topic has no subscribers, no sub-topics and no retained message, it can be removed */
    while(!node->subscribers && tmq_map_size(node->children) == 0 && !node->retain_message)
    {
        topic_tree_node_t* parent = node->parent;
        tmq_map_erase(parent->children, node->level_name);
        topic_tree_node_t_free(node);
        node = parent;
        if(node == topics->root)
            break;
    }
}

void tmq_topics_remove_subscription(tmq_topics_t* topics, char* topic_filter, char* client_id)
{
    topic_tree_node_t* node = add_topic_or_find(topics, topic_filter, 0, NULL);
    if(!node)
    {
        tlog_warn("topic filter doesn't exist: %s", topic_filter);
        return;
    }
    tmq_map_erase(*node->subscribers, client_id);
    if(tmq_map_size(*node->subscribers) == 0)
    {
        tmq_map_free(*node->subscribers);
        free(node->subscribers);
        node->subscribers = NULL;
    }
    try_remove_topic(topics, node);
}

/**
 * @brief Try to match a given topic recursively in the topic tree.
 *
 * @param topics topic tree
 * @param node current topic tree node that matched
 * @param is_multi_wildcard set to 1 if current node is multi-level wildcard '#'
 * @param levels level names of the publish topic that need to match
 *               e.g. topic = "/match/this/topic", levels = ["", "match", "this", "topic"]
 * @param n the index of level that need to be matched
 * @param topic the publish topic that need to be matched
 * @param message mqtt message to publish
 * @param retain set to 1 if this is a retained message
 * */
static void match(tmq_topics_t* topics, topic_tree_node_t* node, int is_multi_wildcard,
                  str_vec* levels, int n, char* topic, mqtt_message* message, int retain)
{
    if(n == tmq_vec_size(*levels) || is_multi_wildcard)
    {
        if(node->subscribers)
            topics->client_on_match(topics->broker, topic, message, node->subscribers);
        member_sub_info_t* member_sub = node->subscribe_members;
        while(member_sub)
        {
            tmq_map_put(topics->matched_members, member_sub->node_addr, 1);
            member_sub = member_sub->next;
        }
        if(n == tmq_vec_size(*levels))
        {
            /* if this is a retained message, save this message under the topic */
            if(retain)
            {
                if(!node->retain_message)
                {
                    node->retain_message = malloc(sizeof(retain_message_t));
                    node->retain_message->retain_msg.message = tmq_str_empty();
                    node->retain_message->retain_topic = tmq_str_empty();
                }
                node->retain_message->retain_msg.message = tmq_str_assign(node->retain_message->retain_msg.message,
                                                                          message->message);
                node->retain_message->retain_msg.qos = message->qos;
                node->retain_message->retain_topic = tmq_str_assign(node->retain_message->retain_topic, topic);
            }
            /* "#" includes the parent. e.g. publish topic "sport/tennis/player1" can match
             * topic filter "sport/tennis/player1/#" */
            topic_tree_node_t** next = tmq_map_get(node->children, "#");
            if(next)
            {
                if((*next)->subscribers)
                    topics->client_on_match(topics->broker, topic, message, (*next)->subscribers);
                member_sub = (*next)->subscribe_members;
                while(member_sub)
                {
                    tmq_map_put(topics->matched_members, member_sub->node_addr, 1);
                    member_sub = member_sub->next;
                }
            }
        }
        return;
    }
    topic_tree_node_t** next;
    char* level = *tmq_vec_at(*levels, n);
    if((next = tmq_map_get(node->children, level)) != NULL)
        match(topics, *next, 0, levels, n + 1, topic, message, retain);
    else if(retain)
    {
        topic_tree_node_t* new_next = topic_tree_node_new(node, level);
        tmq_map_put(node->children, level, new_next);
        match(topics, new_next, 0, levels, n + 1, topic, message, retain);
    }
    if((next = tmq_map_get(node->children, "+")) != NULL)
        match(topics, *next, 0, levels, n + 1, topic, message, 0);
    if((next = tmq_map_get(node->children, "#")) != NULL)
        match(topics, *next, 1, levels, n + 1, topic, message, 0);
}

void tmq_topics_publish(tmq_topics_t* topics, char* topic, mqtt_message* message, int retain)
{
    str_vec levels = tmq_vec_make(tmq_str_t);
    tmq_topic_split(topic, &levels);
    match(topics, topics->root, 0, &levels, 0, topic, message, retain);
    if(tmq_map_size(topics->matched_members) > 0)
    {
        topics->route_on_match(&topics->broker->cluster, topic, message, &topics->matched_members);
        tmq_map_clear(topics->matched_members);
    }
    for(tmq_str_t* it = tmq_vec_begin(levels); it != tmq_vec_end(levels); it++)
        tmq_str_free(*it);
    tmq_vec_free(levels);
}

/* for debugging and testing */
static void topic_info(topic_tree_node_t* node, str_vec* levels)
{
    tmq_map_iter_t it = tmq_map_iter(node->children);
    for(; tmq_map_has_next(it); tmq_map_next(node->children, it))
    {
        char* next_level = it.first;
        topic_tree_node_t* next = *(topic_tree_node_t**) it.second;
        tmq_vec_push_back(*levels, tmq_str_new(next_level));
        topic_info(next, levels);
        tmq_str_free(*tmq_vec_pop_back(*levels));
    }
    if(tmq_map_size(*node->subscribers) > 0 || node->retain_message)
    {
        printf("--------------------\n");
        for(size_t i = 0; i < tmq_vec_size(*levels); i++)
        {
            printf("%s", *tmq_vec_at(*levels, i));
            if(i < tmq_vec_size(*levels) - 1)
                printf("/");
        }
        printf("\nsubscribers:");

        it = tmq_map_iter(*node->subscribers);
        for(; tmq_map_has_next(it); tmq_map_next(*node->subscribers, it))
        {
            char* client_id = it.first;
            uint8_t required_qos = ((subscribe_info_t*)it.second)->qos;
            printf("<%s, %u> ", client_id, required_qos);
        }
        printf("\n");
        if(node->retain_message)
            printf("retain message: %s\n", node->retain_message->retain_msg.message);
    }
}

/* for debugging and testing */
void tmq_topics_info(tmq_topics_t* topics)
{
    str_vec levels = tmq_vec_make(tmq_str_t);
    topic_info(topics->root, &levels);
    printf("--------------------\n");
    tmq_vec_free(levels);
}