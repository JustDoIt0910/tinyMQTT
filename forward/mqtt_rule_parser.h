//
// Created by just do it on 2024/2/2.
//

#ifndef TINYMQTT_MQTT_RULE_PARSER_H
#define TINYMQTT_MQTT_RULE_PARSER_H
#include "base/mqtt_map.h"
#include "mqtt_events.h"

typedef tmq_map(char*, tmq_str_t) select_schema;
typedef struct tmq_rule_parse_result_s
{
    select_schema select_schema_map;
    tmq_event_type event_source;
    tmq_str_t source_topic;
    tmq_filter_expr_t* filter;
} tmq_rule_parse_result_t;

typedef struct tmq_rule_parser_s
{
    event_source_info_t* event_source;
    int error_pos;
} tmq_rule_parser_t;

void tmq_rule_parser_init(tmq_rule_parser_t* parser);
tmq_rule_parse_result_t* tmq_rule_parse(tmq_rule_parser_t* parser, const char* rule);
void tmq_rule_parse_result_free(tmq_rule_parse_result_t* result);
void tmq_rule_parse_result_print(tmq_rule_parse_result_t* result);

#endif //TINYMQTT_MQTT_RULE_PARSER_H
