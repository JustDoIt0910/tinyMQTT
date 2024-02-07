//
// Created by just do it on 2024/2/7.
//
#include "mqtt_rule_engine.h"
#include "mqtt/mqtt_broker.h"
#include "adaptors/mqtt_adaptors.h"

static tmq_event_listener_t* make_event_listener(tmq_rule_parser_t* parser, const char* rule, tmq_event_type* event_source)
{
    tmq_event_listener_t* listener = malloc(sizeof(tmq_event_listener_t));
    bzero(listener, sizeof(tmq_event_listener_t));
    tmq_rule_parse_result_t* result = tmq_rule_parse(parser, rule);
    if(!result)
        return NULL;
    //tmq_rule_parse_result_print(result);
    listener->filter = result->filter;
    listener->adaptor = result->adaptor;
    listener->on_event = result->adaptor->handle_event;
    *event_source = result->event_source;
    tmq_vec_init(&listener->mappings, schema_mapping_item_t);
    tmq_vec_swap(listener->mappings, result->mappings);
    tmq_rule_parse_result_free(result);
    return listener;
}

static void publish_event(tmq_event_listener_t* listener, void* event_data)
{
    if(listener->filter && !listener->filter->evaluate(listener->filter, event_data).boolean)
        return;
    adaptor_value_map parameters;
    adaptor_value_list payload_values = tmq_vec_make(adaptor_value_item_t);
    tmq_map_str_init(&parameters, adaptor_value_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    schema_mapping_item_t* select = tmq_vec_begin(listener->mappings);
    for(; select != tmq_vec_end(listener->mappings); select++)
    {
        filter_expr_value_t value = select->value_expr->evaluate(select->value_expr, event_data);
        if(select->map_to_parameter)
            tmq_map_put(parameters, select->mapping_name, *(adaptor_value_t*)&value);
        else
        {
            adaptor_value_item_t item = {
                    .value_name = select->mapping_name,
                    .value = *(adaptor_value_t*)&value
            };
            tmq_vec_push_back(payload_values, item);
        }
    }
    listener->on_event(listener->adaptor, &parameters, &payload_values);
    for(tmq_map_iter_t iter = tmq_map_iter(parameters); tmq_map_has_next(iter); tmq_map_next(parameters, iter))
    {
        adaptor_value_t* value = iter.second;
        if(value->value_type == ADAPTOR_VALUE_STR)
            tmq_str_free(value->str);
    }
    for(adaptor_value_item_t* it = tmq_vec_begin(payload_values); it != tmq_vec_end(payload_values); it++)
    {
        if(it->value.value_type == ADAPTOR_VALUE_STR)
            tmq_str_free(it->value.str);
    }
    tmq_map_free(parameters);
    tmq_vec_free(payload_values);
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

void tmq_rule_engine_publish_event(tmq_rule_engine_t* engine, tmq_event_t event)
{
    if(event.type != MESSAGE)
    {
        tmq_event_listener_t** listeners = tmq_map_get(engine->listeners, event.type);
        if(!listeners)
            return;
        tmq_event_listener_t* listener = *listeners;
        while(listener)
        {
            publish_event(listener, event.data);
            listener = listener->next;
        }
    }
}