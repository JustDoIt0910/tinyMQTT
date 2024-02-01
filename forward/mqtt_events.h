//
// Created by just do it on 2024/2/1.
//
#ifndef TINYMQTT_MQTT_EVENTS_H
#define TINYMQTT_MQTT_EVENTS_H
#include <cjson//cJSON.h>
#include <stdint.h>
#include "base/mqtt_str.h"

typedef enum tmq_event_type_e
{
    DEVICE_EVENT,
    TOPIC_EVENT,
    PUB_EVENT,
    SUB_UNSUB_EVENT
} tmq_event_type;

typedef struct tmq_event_s
{
    tmq_event_type type;
    void* data;
} tmq_event_t;

typedef struct tmq_pub_event_data_s
{
    tmq_str_t pub_client_id;
    tmq_str_t pub_username;
    uint8_t qos;
    cJSON* payload_as_json;
} tmq_pub_event_data_t;

typedef enum tmq_expr_type_e
{
    VALUE_EXPR,
    BINARY_EXPR
} tmq_expr_type;

typedef struct tmq_filter_expr_s tmq_filter_expr_t;
typedef int (*filter_expr_eval_f)(tmq_filter_expr_t* expr, tmq_pub_event_data_t* event_data);
#define EXPR_PUBLIC_MEMBERS     \
tmq_expr_type expr_type;        \
filter_expr_eval_f evaluate;
typedef struct tmq_filter_expr_s
{
    EXPR_PUBLIC_MEMBERS
} tmq_filter_expr_t;

typedef struct tmq_filter_binary_expr_s
{
    EXPR_PUBLIC_MEMBERS
    tmq_filter_expr_t* left;
    tmq_filter_expr_t* right;
} tmq_filter_binary_expr_t;

typedef struct tmq_event_listener_s
{
    tmq_filter_expr_t* filter;
} tmq_event_listener_t;

#endif //TINYMQTT_MQTT_EVENTS_H
