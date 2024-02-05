//
// Created by just do it on 2024/2/2.
//
#include "forward/mqtt_events.h"
#include "forward/mqtt_rule_parser.h"
#include <stdio.h>

extern tmq_map(char*, event_source_info_t) event_sources_g;

void on_event(void* event_data, void* arg)
{
    printf("handle event\n");
}

int main()
{
    tmq_rule_parser_t parser;
    tmq_event_sources_init();
    tmq_rule_parser_init(&parser);
//    tmq_rule_parse_result_t* result = tmq_rule_parse(&parser, "select qos, username, payload.x as x "
//                                                              "from test/topic/# "
//                                                              "where ((qos==1&&(username==zr||payload.x==abc))||client_id == 123)");
//    tmq_rule_parse_result_t* result = tmq_rule_parse(&parser, "select client_id from {device} where username == zr");
//    if(result)
//    {
//        tmq_rule_parse_result_print(result);
//    }
//    else
//    {
//        printf("syntax error\n");
//    }

    cJSON* payload = cJSON_Parse("{"
                                 "\"string\": \"hello\", "
                                 "\"number\": 100, "
                                 "\"bool\": false"
                                 "}");
    if(!payload)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return 0;
    }

    tmq_pub_event_data_t data = {
            .client_id = tmq_str_new("1234awdfe"),
            .username = tmq_str_new("zr"),
            .qos = 0,
            .payload_as_json = payload
    };

    tmq_event_listener_t* listener = tmq_make_event_listener(&parser,
                                                             "select qos, username, payload.x as x "
                                                             "from test/topic/# "
                                                             "where username == zr && (qos == 0 || payload.bool == true)",
                                                             on_event);
    tmq_publish_event(listener, &data);
}