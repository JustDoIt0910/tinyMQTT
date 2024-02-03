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
    switch(value_expr->value_type)
    {
        case EXPR_VALUE_CLIENT_ID:
            printf("client_id");
            break;
        case EXPR_VALUE_USERNAME:
            printf("username");
            break;
        case EXPR_VALUE_QOS:
            printf("qos");
            break;
        case EXPR_VALUE_PAYLOAD:
            printf("payload.%s", value_expr->payload_field);
            break;
        default: break;
    }
    printf("}");
}

static void print_const_expr(tmq_filter_expr_t* expr)
{
    tmq_filter_const_expr_t* const_expr = (tmq_filter_const_expr_t*)expr;
    printf("%s", const_expr->value);
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

tmq_filter_expr_t* tmq_value_expr_new(tmq_value_expr_type type, const char* payload_field)
{
    tmq_filter_value_expr_t* expr = malloc(sizeof(tmq_filter_value_expr_t));
    bzero(expr, sizeof(tmq_filter_value_expr_t));
    expr->expr_type = VALUE_EXPR;
    expr->value_type = type;
    expr->print = print_value_expr;
    if(type == EXPR_VALUE_PAYLOAD)
        expr->payload_field = tmq_str_new(payload_field);
    return (tmq_filter_expr_t*)expr;
}

tmq_filter_expr_t* tmq_const_expr_new(const char* value)
{
    tmq_filter_const_expr_t* expr = malloc(sizeof(tmq_filter_const_expr_t));
    expr->expr_type = CONST_EXPR;
    expr->print = print_const_expr;
    expr->value = tmq_str_new(value);
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
        tmq_str_free(((tmq_filter_const_expr_t*)expr)->value);
    else if(expr->expr_type == VALUE_EXPR)
        tmq_str_free(((tmq_filter_value_expr_t*)expr)->payload_field);
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