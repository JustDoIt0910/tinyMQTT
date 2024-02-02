//
// Created by just do it on 2024/2/1.
//
#include "mqtt_events.h"
#include <stdlib.h>

tmq_filter_expr_t* tmq_value_expr_new(tmq_value_expr_type type, const char* payload_field)
{
    tmq_filter_value_expr_t* expr = malloc(sizeof(tmq_filter_value_expr_t));
    expr->expr_type = VALUE_EXPR;
    expr->value_type = type;
    if(type == EXPR_VALUE_PAYLOAD)
        expr->payload_field = tmq_str_new(payload_field);
    return (tmq_filter_expr_t*)expr;
}

tmq_filter_expr_t* tmq_const_expr_new(const char* value)
{
    tmq_filter_const_expr_t* expr = malloc(sizeof(tmq_filter_const_expr_t));
    expr->expr_type = CONST_EXPR;
    expr->value = tmq_str_new(value);
    return (tmq_filter_expr_t*)expr;
}

tmq_filter_expr_t* tmq_binary_expr_new(tmq_binary_expr_op op, uint8_t priority)
{
    tmq_filter_binary_expr_t* expr = malloc(sizeof(tmq_filter_binary_expr_t));
    expr->expr_type = BINARY_EXPR;
    expr->op.op = op;
    expr->op.priority = priority;
    return (tmq_filter_expr_t*)expr;
}