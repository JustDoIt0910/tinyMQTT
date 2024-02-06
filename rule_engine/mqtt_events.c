//
// Created by just do it on 2024/2/1.
//
#include "mqtt_events.h"
#include "mqtt_rule_parser.h"
#include "mqtt/mqtt_broker.h"
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

#define get_as(type, data, offset)  *(type*)((char*)(data) + offset)

static filter_expr_value_t value_expr_evaluate(tmq_filter_expr_t* expr, void* event_data)
{
    filter_expr_value_t result;
    tmq_filter_value_expr_t* value_expr = (tmq_filter_value_expr_t*)expr;
    event_data_field_meta_t* meta = value_expr->field_meta;
    result.value_type = meta->value_type;
    if(meta->value_type == INT_VALUE)
    {
        switch(meta->value_size)
        {
            case 1:
                result.integer = get_as(int8_t, event_data, meta->offset);
                break;
            case 2:
                result.integer = get_as(int16_t, event_data, meta->offset);
                break;
            case 4:
                result.integer = get_as(int32_t, event_data, meta->offset);
                break;
            case 8:
                result.integer = get_as(int64_t , event_data, meta->offset);
                break;
        }
    }
    else if(meta->value_type == STR_VALUE)
        result.str = tmq_str_new(get_as(char*, event_data, meta->offset));
    else if(meta->value_type == JSON_VALUE)
    {
        cJSON* json = get_as(cJSON*, event_data, meta->offset);
        cJSON* item = cJSON_GetObjectItemCaseSensitive(json, value_expr->payload_json_field);
        if(!item)
        {
            result.value_type = NULL_VALUE;
            return result;
        }
        if(cJSON_IsString(item))
        {
            result.value_type = STR_VALUE;
            result.str = tmq_str_new(cJSON_GetStringValue(item));
        }
        else if(cJSON_IsNumber(item))
        {
            result.value_type = INT_VALUE;
            result.integer = (int64_t)cJSON_GetNumberValue(item);
        }
        else if(cJSON_IsBool(item))
        {
            result.value_type = BOOL_VALUE;
            result.boolean = cJSON_IsTrue(item);
        }
        else if(cJSON_IsNull(item))
            result.value_type = NULL_VALUE;
    }
    return result;
}

static filter_expr_value_t const_expr_evaluate(tmq_filter_expr_t* expr, void* event_data)
{
    filter_expr_value_t result;
    tmq_filter_const_expr_t* const_expr = (tmq_filter_const_expr_t*)expr;
    result = const_expr->value;
    if(const_expr->value.value_type == STR_VALUE)
        result.str = tmq_str_new(const_expr->value.str);
    return result;
}

static filter_expr_value_t binary_expr_evaluate(tmq_filter_expr_t* expr, void* event_data)
{
    filter_expr_value_t result = {
            .value_type = BOOL_VALUE,
            .boolean = false
    };
    tmq_filter_binary_expr_t *binary_expr = (tmq_filter_binary_expr_t*)expr;
    filter_expr_value_t left_result = binary_expr->left->evaluate(binary_expr->left, event_data);
    filter_expr_value_t right_result = binary_expr->right->evaluate(binary_expr->right, event_data);
    if(left_result.value_type != right_result.value_type)
        goto end;
    switch(binary_expr->op.op)
    {
        case EXPR_OP_EQ:
            if(left_result.value_type == STR_VALUE)
                result.boolean = tmq_str_equal(left_result.str, right_result.str);
            else if(left_result.value_type == INT_VALUE)
                result.boolean = left_result.integer == right_result.integer;
            else if(left_result.value_type == BOOL_VALUE)
                result.boolean = left_result.boolean == right_result.boolean;
            break;
        case EXPR_OP_GT:
            if(left_result.value_type == INT_VALUE)
                result.boolean = left_result.integer > right_result.integer;
            else if(left_result.value_type == STR_VALUE)
                result.boolean = strcmp(left_result.str, right_result.str) > 0;
            break;
        case EXPR_OP_GTE:
            if(left_result.value_type == INT_VALUE)
                result.boolean = left_result.integer >= right_result.integer;
            else if(left_result.value_type == STR_VALUE)
                result.boolean = strcmp(left_result.str, right_result.str) >= 0;
            break;
        case EXPR_OP_LT:
            if(left_result.value_type == INT_VALUE)
                result.boolean = left_result.integer < right_result.integer;
            else if(left_result.value_type == STR_VALUE)
                result.boolean = strcmp(left_result.str, right_result.str) < 0;
            break;
        case EXPR_OP_LTE:
            if(left_result.value_type == INT_VALUE)
                result.boolean = left_result.integer <= right_result.integer;
            else if(left_result.value_type == STR_VALUE)
                result.boolean = strcmp(left_result.str, right_result.str) <= 0;
            break;
        case EXPR_OP_AND:
            if(left_result.value_type != BOOL_VALUE)
                break;
            result.boolean = left_result.boolean && right_result.boolean;
            break;
        case EXPR_OP_OR:
            if(left_result.value_type != BOOL_VALUE)
                break;
            result.boolean = left_result.boolean || right_result.boolean;
            break;
        default: break;
    }
    end:
    if(left_result.value_type == STR_VALUE)
        tmq_str_free(left_result.str);
    if(right_result.value_type == STR_VALUE)
        tmq_str_free(right_result.str);
    return result;
}

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
    expr->evaluate = value_expr_evaluate;
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
    expr->evaluate = const_expr_evaluate;
    expr->print = print_const_expr;
    char* failed_ptr = NULL;
    int64_t integer = strtoll(value, &failed_ptr, 10);
    if(!failed_ptr || !*failed_ptr)
    {
        expr->value.value_type = INT_VALUE;
        expr->value.integer = integer;
    }
    else if(strcmp(value, "true") == 0)
    {
        expr->value.value_type = BOOL_VALUE;
        expr->value.boolean = true;
    }
    else if(strcmp(value, "false") == 0)
    {
        expr->value.value_type = BOOL_VALUE;
        expr->value.boolean = false;
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
    expr->evaluate = binary_expr_evaluate;
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

tmq_event_listener_t* tmq_make_event_listener(tmq_rule_parser_t* parser, const char* rule, tmq_event_type* event_source)
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

void tmq_publish_event(tmq_event_listener_t* listener, void* event_data)
{
    if(listener->filter->evaluate(listener->filter, event_data).boolean)
    {

    }
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