//
// Created by just do it on 2024/2/4.
//

#ifndef TINYMQTT_MQTT_EVENT_DATA_H
#define TINYMQTT_MQTT_EVENT_DATA_H
#include "base/mqtt_str.h"
#include "base/mqtt_map.h"
#include "base/mqtt_util.h"
#include <cjson/cJSON.h>
#include <stdint.h>

typedef enum tmq_event_type_e
{
    DEVICE,
    TOPIC,
    MESSAGE,
    SUBSCRIPTION
} tmq_event_type;

typedef enum tmq_expr_value_type_e
{
    STR_VALUE,
    INT_VALUE,
    BOOL_VALUE,
    JSON_VALUE,
    NULL_VALUE
} tmq_expr_value_type;

typedef struct event_data_field_meta_s
{
    tmq_str_t field_name;
    tmq_expr_value_type value_type;
    size_t value_size;
    unsigned long offset;
} event_data_field_meta_t;

typedef tmq_map(char*, event_data_field_meta_t*) event_data_field_map;

typedef struct event_source_info_s
{
    tmq_event_type source;
    event_data_field_map fields_meta;
} event_source_info_t;


typedef struct tmq_device_event_data_s
{
    enum {ONLINE, OFFLINE} action;
    tmq_str_t client_id;
    tmq_str_t username;
} tmq_device_event_data_t;

typedef struct tmq_topic_event_data_s
{
    enum {ADD, REMOVE} action;
    tmq_str_t topic;
} tmq_topic_event_data_t;

typedef struct tmq_subscription_event_data_s
{
    enum {SUB, UNSUB} action;
    tmq_str_t client_id;
    tmq_str_t username;
    tmq_str_t topic;
    uint8_t sub_qos;
} tmq_subscription_event_data_t;

typedef struct tmq_pub_event_data_s
{
    tmq_str_t client_id;
    tmq_str_t username;
    uint8_t qos;
    cJSON* payload_as_json;
} tmq_pub_event_data_t;

void tmq_event_sources_init();

#define event_data_field(type, member, field_name, field_type)   \
get_field_descriptor_(field_name, field_type, SIZEOF(type, member), OFFSETOF(type, member))

#define register_event_source(event_source, source_name, ...) \
register_event_source_(event_source, source_name, __VA_ARGS__, NULL)

event_data_field_meta_t* get_field_descriptor_(char* field_name, tmq_expr_value_type field_type, size_t field_size,
                                               unsigned long offset);
void register_event_source_(tmq_event_type event_source, char* source_name, ...);

#endif //TINYMQTT_MQTT_EVENT_DATA_H
