//
// Created by just do it on 2024/2/1.
//
#include "mqtt_events.h"
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

static void print_value_expr(tmq_filter_expr_t* expr)
{
    tmq_filter_value_expr_t* value_expr = (tmq_filter_value_expr_t*)expr;
    printf("${");
    if(value_expr->field_meta->value_type != JSON_VALUE)
        printf("%s", value_expr->field_meta->field_name);
    else
        printf("payload.%s", value_expr->payload_json_field);
    printf("}");
}

static void print_const_expr(tmq_filter_expr_t* expr)
{
    tmq_filter_const_expr_t* const_expr = (tmq_filter_const_expr_t*)expr;
    if(const_expr->value.value_type == STR_VALUE)
        printf("%s", const_expr->value.str);
    else printf("%ld", const_expr->value.integer);
}

static void print_binary_expr(tmq_filter_expr_t* expr)
{
    tmq_filter_binary_expr_t* binary_expr = (tmq_filter_binary_expr_t*)expr;
    switch(binary_expr->op.op)
    {
        case EXPR_OP_EQ:
            printf("==");
            break;
        case EXPR_OP_GT:
            printf(">");
            break;
        case EXPR_OP_GTE:
            printf(">=");
            break;
        case EXPR_OP_LT:
            printf("<");
            break;
        case EXPR_OP_LTE:
            printf("<=");
            break;
        case EXPR_OP_AND:
            printf("and");
            break;
        case EXPR_OP_OR:
            printf("or");
            break;
        default: break;
    }
}

tmq_filter_expr_t* tmq_value_expr_new(event_data_field_meta_t* field_meta, const char* payload_field)
{
    tmq_filter_value_expr_t* expr = malloc(sizeof(tmq_filter_value_expr_t));
    bzero(expr, sizeof(tmq_filter_value_expr_t));
    expr->expr_type = VALUE_EXPR;
    expr->print = print_value_expr;
    expr->field_meta = field_meta;
    if(expr->field_meta->value_type == JSON_VALUE)
        expr->payload_json_field = tmq_str_new(payload_field);
    return (tmq_filter_expr_t*)expr;
}

tmq_filter_expr_t* tmq_const_expr_new(tmq_str_t value)
{
    tmq_filter_const_expr_t* expr = malloc(sizeof(tmq_filter_const_expr_t));
    expr->expr_type = CONST_EXPR;
    expr->print = print_const_expr;
    char* failed_ptr = NULL;
    int64_t integer = strtoll(value, &failed_ptr, 10);
    if(!failed_ptr)
    {
        expr->value.value_type = INT_VALUE;
        expr->value.integer = integer;
    }
    else
    {
        expr->value.value_type = STR_VALUE;
        expr->value.str = tmq_str_new(value);
    }
    return (tmq_filter_expr_t*)expr;
}

tmq_filter_expr_t* tmq_binary_expr_new(tmq_binary_expr_op op, uint8_t priority)
{
    tmq_filter_binary_expr_t* expr = malloc(sizeof(tmq_filter_binary_expr_t));
    expr->expr_type = BINARY_EXPR;
    expr->print = print_binary_expr;
    expr->op.op = op;
    expr->op.priority = priority;
    return (tmq_filter_expr_t*)expr;
}

void tmq_expr_free(tmq_filter_expr_t* expr)
{
    if(expr->expr_type == CONST_EXPR)
    {
        if(((tmq_filter_const_expr_t*)expr)->value.value_type == STR_VALUE)
            tmq_str_free(((tmq_filter_const_expr_t*)expr)->value.str);
    }
    else if(expr->expr_type == VALUE_EXPR)
        tmq_str_free(((tmq_filter_value_expr_t*)expr)->payload_json_field);
    free(expr);
}

void tmq_print_filter_inorder(tmq_filter_expr_t* filter)
{
    if(filter->expr_type != BINARY_EXPR)
    {
        filter->print(filter);
        printf(" ");
        return;
    }
    tmq_print_filter_inorder(((tmq_filter_binary_expr_t*)filter)->left);
    filter->print(filter);
    printf(" ");
    tmq_print_filter_inorder(((tmq_filter_binary_expr_t*)filter)->right);
}

void tmq_print_filter_preorder(tmq_filter_expr_t* filter)
{
    if(filter->expr_type != BINARY_EXPR)
    {
        filter->print(filter);
        printf(" ");
        return;
    }
    filter->print(filter);
    printf(" ");
    tmq_print_filter_preorder(((tmq_filter_binary_expr_t*)filter)->left);
    tmq_print_filter_preorder(((tmq_filter_binary_expr_t*)filter)->right);
}