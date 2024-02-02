//
// Created by just do it on 2024/2/1.
//
#ifndef TINYMQTT_MQTT_EVENTS_H
#define TINYMQTT_MQTT_EVENTS_H
#include <cjson/cJSON.h>
#include <stdint.h>
#include <stdbool.h>
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
    BINARY_EXPR,
    CONST_EXPR
} tmq_expr_type;

typedef union filter_expr_result_u
{
    char* str;
    int64_t integer;
    bool is_true;
} filter_expr_result;

typedef struct tmq_filter_expr_s tmq_filter_expr_t;
typedef filter_expr_result (*filter_expr_eval_f)(tmq_filter_expr_t* expr, tmq_pub_event_data_t* event_data);
typedef void (*filter_expr_print_f)(tmq_filter_expr_t* expr);

#define EXPR_PUBLIC_MEMBERS     \
tmq_expr_type expr_type;        \
filter_expr_eval_f evaluate;    \
filter_expr_print_f print;

typedef struct tmq_filter_expr_s
{
    EXPR_PUBLIC_MEMBERS
} tmq_filter_expr_t;

typedef struct tmq_filter_const_expr_s
{
    EXPR_PUBLIC_MEMBERS
    tmq_str_t value;
} tmq_filter_const_expr_t;

typedef enum tmq_value_expr_type_e
{
    EXPR_VALUE_CLIENT_ID,
    EXPR_VALUE_USERNAME,
    EXPR_VALUE_QOS,
    EXPR_VALUE_PAYLOAD,
    EXPR_VALUE_INVALID
} tmq_value_expr_type;

typedef struct tmq_filter_value_expr_s
{
    EXPR_PUBLIC_MEMBERS
    tmq_value_expr_type value_type;
    tmq_str_t payload_field;
} tmq_filter_value_expr_t;

typedef enum tmq_binary_expr_op_e
{
    EXPR_OP_EQ,
    EXPR_OP_GT,
    EXPR_OP_GTE,
    EXPR_OP_LT,
    EXPR_OP_LTE,
    EXPR_OP_AND,
    EXPR_OP_OR,
    EXPR_OP_LP,
    EXPR_OP_RP
} tmq_binary_expr_op;

typedef struct operator_into_s
{
    tmq_binary_expr_op op;
    uint8_t priority;
} operator_into_t;

typedef struct tmq_filter_binary_expr_s
{
    EXPR_PUBLIC_MEMBERS
    operator_into_t op;
    tmq_filter_expr_t* left;
    tmq_filter_expr_t* right;
} tmq_filter_binary_expr_t;

typedef void (*on_event_f)(tmq_event_t* event, void* arg);

typedef struct tmq_event_listener_s
{
    struct tmq_event_listener_s* next;
    tmq_filter_expr_t* filter;
    on_event_f on_event;
} tmq_event_listener_t;

tmq_filter_expr_t* tmq_value_expr_new(tmq_value_expr_type type, const char* payload_field);
tmq_filter_expr_t* tmq_const_expr_new(const char* value);
tmq_filter_expr_t* tmq_binary_expr_new(tmq_binary_expr_op op, uint8_t priority);
void tmq_print_filter_inorder(tmq_filter_expr_t* filter);
void tmq_print_filter_preorder(tmq_filter_expr_t* filter);

#endif //TINYMQTT_MQTT_EVENTS_H
