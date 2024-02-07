//
// Created by just do it on 2024/2/2.
//

#ifndef TINYMQTT_MQTT_RULE_PARSER_H
#define TINYMQTT_MQTT_RULE_PARSER_H
#include "base/mqtt_map.h"
#include "mqtt_events.h"
#include "adaptors/mqtt_adaptors.h"

typedef struct tmq_rule_parse_result_s
{
    schema_mapping_list mappings;
    tmq_adaptor_t* adaptor;
    tmq_event_type event_source;
    tmq_str_t source_topic;
    tmq_filter_expr_t* filter;
    bool need_json_payload;
} tmq_rule_parse_result_t;

typedef struct tmq_broker_s tmq_broker_t;
typedef struct tmq_rule_parser_s
{
    tmq_broker_t* broker;
    event_source_info_t* event_source;
    int error_pos;
    tmq_str_t error_info;
} tmq_rule_parser_t;

void tmq_rule_parser_init(tmq_rule_parser_t* parser, tmq_broker_t* broker);
tmq_rule_parse_result_t* tmq_rule_parse(tmq_rule_parser_t* parser, const char* rule);
void tmq_rule_parse_result_free(tmq_rule_parse_result_t* result);
void tmq_rule_parse_result_print(tmq_rule_parse_result_t* result);

#endif //TINYMQTT_MQTT_RULE_PARSER_H
