//
// Created by just do it on 2024/1/13.
//
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "mqtt_acl.h"
#include "mqtt_topic.h"
#include "mqtt_session.h"
#include "base/mqtt_str.h"
#include "base/mqtt_util.h"

static acl_tree_node_t* acl_tree_node_new()
{
    acl_tree_node_t* node = malloc(sizeof(acl_tree_node_t));
    if(!node) fatal_error("malloc() error: out of memory");
    bzero(node, sizeof(acl_tree_node_t));
    tmq_map_str_init(&node->children, acl_tree_node_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    return node;
}

static acl_tree_node_t* find_or_create(acl_tree_node_t* cur, char* level)
{
    acl_tree_node_t** next = tmq_map_get(cur->children, level);
    if(next) return *next;
    acl_tree_node_t* new_next = acl_tree_node_new();
    tmq_map_put(cur->children, level, new_next);
    return new_next;
}

static acl_tree_node_t* add_topic_or_find(tmq_acl_t* acl, char* topic_filter)
{
    char* lp = topic_filter, *rp = lp;
    acl_tree_node_t* node = acl->root;
    tmq_str_t level = tmq_str_empty();
    while(1)
    {
        while(*rp && *rp != '/') rp++;
        if(rp == lp)
        {
            level = tmq_str_assign(level, "");
            node = find_or_create(node, level);
            if(!node) break;
        }
        else
        {
            level = tmq_str_assign_n(level, lp, rp - lp);
            node = find_or_create(node, level);
            if(!node) break;
        }
        if(!*rp) break;
        lp = rp + 1; rp = lp;
        if(!*lp)
        {
            level = tmq_str_assign(level, "");
            node = find_or_create(node, level);
            break;
        }
    }
    tmq_str_free(level);
    return node;
}

static void add_rule(acl_rule_t** existing, acl_rule_t* new)
{
    acl_rule_t** r = existing;
    int merged = 0;
    while(*r)
    {
        if((*r)->check_same_target(*r, new))
        {
            // check if there are conflicts with existing rules
            if((*r)->permission != new->permission &&
            ((*r)->access == new->access || (*r)->access == PUB_SUB || new->access == PUB_SUB))
            {
                acl_rule_t* conflict = *r;
                acl_rule_t* next = conflict->next;
                free(conflict);
                *r = next;
            }
            // merge with existing rule
            if((*r)->permission == new->permission && (*r)->access != new->access)
            {
                (*r)->access = PUB_SUB;
                merged = 1;
            }
        }
        r = &(*r)->next;
    }
    if(merged)
    {
        free(new);
        return;
    }
    new->next = *existing;
    *existing = new;
}

void tmq_acl_init(tmq_acl_t* acl, tmq_permission_e nomatch)
{
    bzero(acl, sizeof(tmq_acl_t));
    pthread_rwlock_init(&acl->lk, NULL);
    acl->root = acl_tree_node_new();
    acl->nomatch_permission = nomatch;
}

void tmq_acl_add_rule(tmq_acl_t* acl, char* topic_filter, acl_rule_t* rule)
{
    pthread_rwlock_wrlock(&acl->lk);
    acl_tree_node_t* node = add_topic_or_find(acl, topic_filter);
    assert(node != NULL);
    add_rule(&node->rules, rule);
    pthread_rwlock_unlock(&acl->lk);
}

void tmq_acl_add_rule_for_all(tmq_acl_t* acl, char* topic_filter, acl_rule_t* rule)
{
    pthread_rwlock_wrlock(&acl->lk);
    acl_tree_node_t* node = add_topic_or_find(acl, topic_filter);
    assert(node != NULL);
    add_rule(&node->rules_for_all, rule);
    pthread_rwlock_unlock(&acl->lk);
}

static acl_tree_node_t* match(acl_tree_node_t* cur, int n, int is_multi_wildcard, str_vec* levels)
{
    if(n == tmq_vec_size(*levels) || is_multi_wildcard)
        return cur;
    acl_tree_node_t** next;
    acl_tree_node_t* matched = NULL;
    char* level = *tmq_vec_at(*levels, n);
    const char* single_wildcard = "+";
    const char* multi_wildcard = "#";
    // can't use macro tmq_map_get() here because it is not thread-safe for reading
    if((next = tmq_map_get_(cur->children.base, &level)) != NULL)
        matched = match(*next, n + 1, 0, levels);
    if(!matched && (next = tmq_map_get_(cur->children.base, &single_wildcard)) != NULL)
        matched = match(*next, n + 1, 0, levels);
    if(!matched && (next = tmq_map_get_(cur->children.base, &multi_wildcard)) != NULL)
        matched = match(*next, n + 1, 1, levels);
    return matched;
}

tmq_permission_e tmq_acl_auth(tmq_acl_t* acl, tmq_session_t* session, char* topic_filter, tmq_access_e access)
{
    str_vec levels = tmq_vec_make(tmq_str_t);
    tmq_topic_split(topic_filter, &levels);
    pthread_rwlock_rdlock(&acl->lk);
    acl_tree_node_t* node = match(acl->root, 0, 0, &levels);
    for(tmq_str_t* it = tmq_vec_begin(levels); it != tmq_vec_end(levels); it++)
        tmq_str_free(*it);
    tmq_vec_free(levels);
    if(!node)
        return acl->nomatch_permission;
    tmq_permission_e perm = UNKNOWN;
    acl_rule_t* r = node->rules;
    while(r)
    {
        perm = r->check_permission(r, session, access);
        if(perm != UNKNOWN)
            break;
        r = r->next;
    }
    if(perm == UNKNOWN)
    {
        r = node->rules_for_all;
        while(r)
        {
            perm = r->check_permission(r, session, access);
            if(perm != UNKNOWN)
                break;
            r = r->next;
        }
    }
    pthread_rwlock_unlock(&acl->lk);
    return perm == UNKNOWN ? acl->nomatch_permission : perm;
}

// ***************************** acl member functions **********************************

static tmq_permission_e client_id_rule_check_permission(acl_rule_t* rule, tmq_session_t* session, tmq_access_e access)
{
    acl_client_id_rule_t* client_id_rule = (acl_client_id_rule_t*)rule;
    if(client_id_rule->access != PUB_SUB && client_id_rule->access != access)
        return UNKNOWN;
    if(tmq_str_equal(client_id_rule->client_id, session->client_id))
        return client_id_rule->permission;
    return UNKNOWN;
}

static int client_id_rule_check_same_target(acl_rule_t* rule, acl_rule_t* other_rule)
{
    if(rule->check_permission != other_rule->check_permission)
        return 0;
    acl_client_id_rule_t* client_id_rule = (acl_client_id_rule_t*)rule;
    acl_client_id_rule_t* other_username_rule = (acl_client_id_rule_t*)other_rule;
    return tmq_str_equal(client_id_rule->client_id, other_username_rule->client_id);
}

static tmq_permission_e username_rule_check_permission(acl_rule_t* rule, tmq_session_t* session, tmq_access_e access)
{
    acl_username_rule_t* username_rule = (acl_username_rule_t*)rule;
    if(username_rule->access != PUB_SUB && username_rule->access != access)
        return UNKNOWN;
    if(tmq_str_equal(username_rule->username, session->username))
        return username_rule->permission;
    return UNKNOWN;
}

static int username_rule_check_same_target(acl_rule_t* rule, acl_rule_t* other_rule)
{
    if(rule->check_permission != other_rule->check_permission)
        return 0;
    acl_username_rule_t* username_rule = (acl_username_rule_t*)rule;
    acl_username_rule_t* other_username_rule = (acl_username_rule_t*)other_rule;
    return tmq_str_equal(username_rule->username, other_username_rule->username);
}

static tmq_permission_e ip_rule_check_permission(acl_rule_t* rule, tmq_session_t* session, tmq_access_e access)
{
    acl_ip_rule_t* ip_rule = (acl_ip_rule_t*)rule;
    if(ip_rule->access != PUB_SUB && ip_rule->access != access)
        return UNKNOWN;
    struct sockaddr_in peer_addr;
    tmq_socket_peer_addr(session->conn->fd, &peer_addr);
    if(ip_rule->addr.s_addr == peer_addr.sin_addr.s_addr)
        return ip_rule->permission;
    return UNKNOWN;
}

static int ip_rule_check_same_target(acl_rule_t* rule, acl_rule_t* other_rule)
{
    if(rule->check_permission != other_rule->check_permission)
        return 0;
    acl_ip_rule_t* ip_rule = (acl_ip_rule_t*)rule;
    acl_ip_rule_t* other_ip_rule = (acl_ip_rule_t*)other_rule;
    return ip_rule->addr.s_addr == other_ip_rule->addr.s_addr;
}

static tmq_permission_e all_rule_check_permission(acl_rule_t* rule, tmq_session_t* session, tmq_access_e access)
{
    acl_all_rule_t* all_rule = (acl_all_rule_t*)rule;
    if(all_rule->access != PUB_SUB && all_rule->access != access)
        return UNKNOWN;
    return all_rule->permission;
}

static int all_rule_check_same_target(acl_rule_t* rule, acl_rule_t* other_rule)
{
    return rule->check_permission == other_rule->check_permission;
}

acl_rule_t* acl_client_id_rule_new(tmq_permission_e permission, const char* client_id, tmq_access_e access)
{
    acl_client_id_rule_t* rule = malloc(sizeof(acl_client_id_rule_t));
    rule->client_id = tmq_str_new(client_id);
    rule->permission = permission;
    rule->access = access;
    rule->check_permission = client_id_rule_check_permission;
    rule->check_same_target = client_id_rule_check_same_target;
    return (acl_rule_t*)rule;
}

acl_rule_t* acl_username_rule_new(tmq_permission_e permission, const char* username, tmq_access_e access)
{
    acl_username_rule_t* rule = malloc(sizeof(acl_username_rule_t));
    rule->username = tmq_str_new(username);
    rule->permission = permission;
    rule->access = access;
    rule->check_permission = username_rule_check_permission;
    rule->check_same_target = username_rule_check_same_target;
    return (acl_rule_t*)rule;
}

acl_rule_t* acl_ip_rule_new(tmq_permission_e permission, const char* ip, tmq_access_e access)
{
    acl_ip_rule_t* rule = malloc(sizeof(acl_ip_rule_t));
    struct sockaddr_in addr = tmq_addr_from_ip_port(ip, 0);
    rule->addr = addr.sin_addr;
    rule->permission = permission;
    rule->access = access;
    rule->check_permission = ip_rule_check_permission;
    rule->check_same_target = ip_rule_check_same_target;
    return (acl_rule_t*)rule;
}

acl_rule_t* acl_all_rule_new(tmq_permission_e permission, tmq_access_e access)
{
    acl_all_rule_t* rule = malloc(sizeof(acl_username_rule_t));
    rule->permission = permission;
    rule->access = access;
    rule->check_permission = all_rule_check_permission;
    rule->check_same_target = all_rule_check_same_target;
    return (acl_rule_t*)rule;
}