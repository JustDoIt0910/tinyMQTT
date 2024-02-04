//
// Created by just do it on 2024/2/2.
//
#include "forward/mqtt_events.h"
#include "forward/mqtt_rule_parser.h"
#include <stdio.h>

int main()
{
    tmq_rule_parser_t parser;
    tmq_event_sources_init();
    tmq_rule_parser_init(&parser);
//    tmq_rule_parse_result_t* result = tmq_rule_parse(&parser, "select qos, username, payload.x as x "
//                                                              "from test/topic/# "
//                                                              "where ((qos==1&&(username==zr||payload.x==abc))||client_id == 123)");
    tmq_rule_parse_result_t* result = tmq_rule_parse(&parser, "select client_id from {device} where username == zr");
    if(result)
    {
        tmq_rule_parse_result_print(result);
    }
    else
    {
        printf("syntax error\n");
    }
}