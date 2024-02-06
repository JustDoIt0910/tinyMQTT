//
// Created by just do it on 2024/2/1.
//
#ifndef TINYMQTT_MQTT_EVENTS_H
#define TINYMQTT_MQTT_EVENTS_H
#include <stdbool.h>
#include <stdint.h>
#include "base/mqtt_str.h"
#include "mqtt_event_source.h"
#include "adaptors/mqtt_adaptors.h"

typedef struct tmq_event_s
{
    tmq_event_type type;
    void* data;
} tmq_event_t;

typedef enum tmq_expr_type_e
{
    VALUE_EXPR,
    BINARY_EXPR,
    CONST_EXPR
} tmq_expr_type;

typedef struct filter_expr_value_s
{
    tmq_expr_value_type value_type;
    union
    {
        tmq_str_t str;
        int64_t integer;
        bool boolean;
    };
} filter_expr_value_t;

typedef struct tmq_filter_expr_s tmq_filter_expr_t;
typedef filter_expr_value_t (*filter_expr_eval_f)(tmq_filter_expr_t* expr, void* event_data);
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
    filter_expr_value_t value;
} tmq_filter_const_expr_t;

typedef struct tmq_filter_value_expr_s
{
    EXPR_PUBLIC_MEMBERS
    event_data_field_meta_t* field_meta;
    tmq_str_t payload_json_field;
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

typedef struct schema_mapping_item_s
{
    tmq_filter_expr_t* value_expr;
    bool map_to_parameter;
    char mapping_name[50];
    adaptor_value_type mapping_type;
} schema_mapping_item_t;

typedef tmq_vec(schema_mapping_item_t) schema_mapping_list;

typedef struct tmq_event_listener_s
{
    struct tmq_event_listener_s* next;
    schema_mapping_list mappings;
    tmq_filter_expr_t* filter;
    handle_event_f on_event;
} tmq_event_listener_t;

typedef struct tmq_rule_parser_s tmq_rule_parser_t;
tmq_event_listener_t* tmq_make_event_listener(tmq_rule_parser_t* parser, const char* rule, tmq_event_type* event_source);
void tmq_publish_event(tmq_event_listener_t* listener, void* event_data);

tmq_filter_expr_t* tmq_value_expr_new(event_data_field_meta_t* field_meta, const char* payload_field);
tmq_filter_expr_t* tmq_const_expr_new(tmq_str_t value);
tmq_filter_expr_t* tmq_binary_expr_new(tmq_binary_expr_op op, uint8_t priority);
void tmq_expr_free(tmq_filter_expr_t* expr);
void tmq_print_filter_inorder(tmq_filter_expr_t* filter);
void tmq_print_filter_preorder(tmq_filter_expr_t* filter);

#endif //TINYMQTT_MQTT_EVENTS_H
