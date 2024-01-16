//
// Created by just do it on 2024/1/15.
//
#include "mqtt/mqtt_acl.h"
#include "mqtt/mqtt_session.h"
#include <stdio.h>

int main()
{
    tmq_acl_t acl;
    tmq_acl_init(&acl, ALLOW);
    tmq_acl_add_rule(&acl, "test/#", acl_username_rule_new(DENY, "user1", SUB));
    tmq_acl_add_rule_for_all(&acl, "test/#", acl_all_rule_new(ALLOW, SUB));

    tmq_session_t* session = tmq_session_new(NULL, NULL, NULL, NULL,
                                                 NULL, 1, 0, NULL,
                                                 NULL, 0, 0, 0, NULL);
    session->username = tmq_str_new("user2");
    printf("%d\n", tmq_acl_auth(&acl, session, "test/a/b", SUB));
}