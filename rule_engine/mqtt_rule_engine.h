//
// Created by just do it on 2024/2/7.
//

#ifndef TINYMQTT_MQTT_RULE_ENGINE_H
#define TINYMQTT_MQTT_RULE_ENGINE_H
#include "mqtt_rule_parser.h"

typedef struct tmq_topics_s tmq_topics_t;

typedef struct tmq_rule_engine_s
{
    tmq_rule_parser_t parser;
    tmq_map(tmq_event_type, tmq_event_listener_t*) listeners;
    tmq_topics_t* topic_tree;
} tmq_rule_engine_t;

void tmq_rule_engine_init(tmq_rule_engine_t* engine, tmq_broker_t* broker);
void tmq_rule_engine_add_rule(tmq_rule_engine_t* engine, const char* rule);

#endif //TINYMQTT_MQTT_RULE_ENGINE_H
