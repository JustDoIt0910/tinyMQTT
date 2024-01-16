//
// Created by just do it on 2024/1/13.
//

#ifndef TINYMQTT_ACL_H
#define TINYMQTT_ACL_H
#include <pthread.h>
#include <netinet/in.h>
#include "base/mqtt_map.h"

typedef struct tmq_session_s tmq_session_t;
typedef char* tmq_str_t;
typedef enum tmq_permission_e {ALLOW, DENY, UNKNOWN} tmq_permission_e;
static const char* permission_str[] = {"allow", "deny"};
typedef enum tmq_access_e {PUB, SUB, PUB_SUB} tmq_access_e;
static const char* access_str[] = {"publish", "subscribe", "publish/subscribe"};

typedef struct acl_rule_s acl_rule_t;
typedef tmq_permission_e(*permission_check_f)(acl_rule_t* rule, tmq_session_t* session, tmq_access_e access);
typedef int(*same_target_heck_f)(acl_rule_t* rule, acl_rule_t* other_rule);
#define ACL_RULE_PUBLIC_MEMBERS         \
struct acl_rule_s* next;                \
tmq_access_e access;                    \
tmq_permission_e permission;            \
permission_check_f check_permission;    \
same_target_heck_f check_same_target

typedef struct acl_rule_s {ACL_RULE_PUBLIC_MEMBERS;} acl_rule_t;
typedef struct acl_all_rule_s {ACL_RULE_PUBLIC_MEMBERS;} acl_all_rule_t;
typedef struct acl_username_rule_s
{
    ACL_RULE_PUBLIC_MEMBERS;
    tmq_str_t username;
} acl_username_rule_t;

typedef struct acl_client_id_rule_s
{
    ACL_RULE_PUBLIC_MEMBERS;
    tmq_str_t client_id;
} acl_client_id_rule_t;

typedef struct acl_ip_rule_s
{
    ACL_RULE_PUBLIC_MEMBERS;
    struct in_addr addr;
} acl_ip_rule_t;

acl_rule_t* acl_client_id_rule_new(tmq_permission_e permission, const char* client_id, tmq_access_e access);
acl_rule_t* acl_username_rule_new(tmq_permission_e permission, const char* username, tmq_access_e access);
acl_rule_t* acl_ip_rule_new(tmq_permission_e permission, const char* ip, tmq_access_e access);
acl_rule_t* acl_all_rule_new(tmq_permission_e permission, tmq_access_e access);

typedef struct acl_tree_node_s acl_tree_node_t;
typedef tmq_map(char*, acl_tree_node_t*) children_map_t;
typedef struct acl_tree_node_s
{
    acl_rule_t* rules;
    acl_rule_t* rules_for_all;
    children_map_t children;
} acl_tree_node_t;

typedef struct tmq_acl_s
{
    pthread_rwlock_t lk;
    acl_tree_node_t* root;
    tmq_permission_e nomatch_permission;
} tmq_acl_t;

void tmq_acl_init(tmq_acl_t* acl, tmq_permission_e nomatch);
void tmq_acl_add_rule(tmq_acl_t* acl, char* topic_filter, acl_rule_t* rule);
void tmq_acl_add_rule_for_all(tmq_acl_t* acl, char* topic_filter, acl_rule_t* rule);
tmq_permission_e tmq_acl_auth(tmq_acl_t* acl, tmq_session_t* session, char* topic_filter, tmq_access_e access);

#endif //TINYMQTT_ACL_H
