//
// Created by just do it on 2024/2/4.
//
#include "mqtt_event_source.h"
#include <stdarg.h>
#include <assert.h>

tmq_map(char*, event_source_info_t) event_sources_g;

void tmq_event_sources_init()
{
    tmq_map_str_init(&event_sources_g, event_source_info_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    register_event_source(DEVICE, "device",
                          event_data_field(tmq_device_event_data_t, action, "action", INT_VALUE),
                          event_data_field(tmq_device_event_data_t, client_id, "client_id", STR_VALUE),
                          event_data_field(tmq_device_event_data_t, username, "username", STR_VALUE));
    register_event_source(DEVICE, "topic",
                          event_data_field(tmq_topic_event_data_t, action, "action", INT_VALUE),
                          event_data_field(tmq_topic_event_data_t, topic, "topic", STR_VALUE));
    register_event_source(SUBSCRIPTION, "subscription",
                          event_data_field(tmq_subscription_event_data_t, action, "action", INT_VALUE),
                          event_data_field(tmq_subscription_event_data_t, client_id, "client_id", STR_VALUE),
                          event_data_field(tmq_subscription_event_data_t, username, "username", STR_VALUE),
                          event_data_field(tmq_subscription_event_data_t, topic, "topic", STR_VALUE),
                          event_data_field(tmq_subscription_event_data_t, sub_qos, "sub_qos", INT_VALUE));
    register_event_source(MESSAGE, "message",
                          event_data_field(tmq_pub_event_data_t, client_id, "client_id", STR_VALUE),
                          event_data_field(tmq_pub_event_data_t, username, "username", STR_VALUE),
                          event_data_field(tmq_pub_event_data_t, qos, "qos", INT_VALUE),
                          event_data_field(tmq_pub_event_data_t, payload_as_json, "payload", JSON_VALUE));
}

event_data_field_meta_t* get_field_descriptor_(char* field_name, tmq_expr_value_type field_type, size_t field_size,
                                               unsigned long offset)
{
    event_data_field_meta_t* field_meta = malloc(sizeof(event_data_field_meta_t));
    field_meta->field_name = tmq_str_new(field_name);
    field_meta->value_type = field_type;
    field_meta->value_size = field_size;
    field_meta->offset = offset;
    return field_meta;
}

void register_event_source_(tmq_event_type event_source, char* source_name, ...)
{
    event_source_info_t* event_source_info = tmq_map_get(event_sources_g, source_name);
    if(!event_source_info)
    {
        event_source_info_t info;
        info.source = event_source;
        tmq_map_str_init(&info.fields_meta, event_data_field_meta_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
        tmq_map_put(event_sources_g, source_name, info);
        event_source_info = tmq_map_get(event_sources_g, source_name);
        assert(event_source_info != NULL);
    }
    va_list va;
    va_start(va, source_name);
    while(1)
    {
        event_data_field_meta_t* field_meta = va_arg(va, event_data_field_meta_t*);
        if(!field_meta)
            break;
        tmq_map_put(event_source_info->fields_meta, field_meta->field_name, field_meta);
    }
    va_end(va);
}