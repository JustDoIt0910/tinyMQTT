//
// Created by just do it on 2024/2/7.
//
#include "mqtt_rule_engine.h"
#include "mqtt/mqtt_broker.h"

static tmq_event_listener_t* make_event_listener(tmq_rule_parser_t* parser, const char* rule, tmq_event_type* event_source)
{
    tmq_event_listener_t* listener = malloc(sizeof(tmq_event_listener_t));
    bzero(listener, sizeof(tmq_event_listener_t));
    tmq_rule_parse_result_t* result = tmq_rule_parse(parser, rule);
    if(!result)
        return NULL;
    //tmq_rule_parse_result_print(result);
    listener->filter = result->filter;
    listener->on_event = result->adaptor->handle_event;
    *event_source = result->event_source;
    tmq_vec_init(&listener->mappings, schema_mapping_item_t);
    tmq_vec_swap(listener->mappings, result->mappings);
    tmq_rule_parse_result_free(result);
    return listener;
}

static void publish_event(tmq_event_listener_t* listener, void* event_data)
{
    if(listener->filter->evaluate(listener->filter, event_data).boolean)
    {

    }
}

void tmq_rule_engine_init(tmq_rule_engine_t* engine, tmq_broker_t* broker)
{
    tmq_rule_parser_init(&engine->parser, broker);
    tmq_map_32_init(&engine->listeners, tmq_event_listener_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    engine->topic_tree = &broker->topics_tree;
}

void tmq_rule_engine_add_rule(tmq_rule_engine_t* engine, const char* rule)
{
    tmq_event_type event_source;
    tmq_event_listener_t* listener = make_event_listener(&engine->parser, rule, &event_source);
    if(listener)
    {
        if(event_source != MESSAGE)
        {
            tmq_event_listener_t** listeners = tmq_map_get(engine->listeners, event_source);
            if(!listeners)
                tmq_map_put(engine->listeners, event_source, listener);
            else
            {
                listener->next = *listeners;
                *listeners = listener;
            }
        }
        else
        {

        }
    }
    else
    {
        tlog_info("parse error at %d: %s", engine->parser.error_pos, engine->parser.error_info);
    }
    tmq_str_free(engine->parser.error_info);
    engine->parser.error_pos = 0;
    engine->parser.event_source = NULL;
}