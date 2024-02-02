//
// Created by just do it on 2024/2/2.
//
#include "mqtt_rule_parser.h"
#include "base/mqtt_map.h"
#include "mqtt_events.h"
#include <ctype.h>

static tmq_map(char*, operator_into_t) operators;

void tmq_rule_parser_init()
{
    tmq_map_str_init(&operators, operator_into_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    operator_into_t eq = {.op = EXPR_OP_EQ, .priority = 3};
    operator_into_t gt = {.op = EXPR_OP_GT, .priority = 3};
    operator_into_t gte = {.op = EXPR_OP_GTE, .priority = 3};
    operator_into_t lt = {.op = EXPR_OP_LT, .priority = 3};
    operator_into_t lte = {.op = EXPR_OP_LTE, .priority = 3};
    operator_into_t and = {.op = EXPR_OP_AND, .priority = 2};
    operator_into_t or = {.op = EXPR_OP_OR, .priority = 2};
    operator_into_t left_parentheses = { .priority = 1};
    operator_into_t right_parentheses = { .priority = 1};
    operator_into_t end_mark = { .priority = 0};
    tmq_map_put(operators, "==", eq);
    tmq_map_put(operators, ">", gt);
    tmq_map_put(operators, ">=", gte);
    tmq_map_put(operators, "<", lt);
    tmq_map_put(operators, "<=", lte);
    tmq_map_put(operators, "&&", and);
    tmq_map_put(operators, "||", or);
    tmq_map_put(operators, "(", left_parentheses);
    tmq_map_put(operators, ")", right_parentheses);
    tmq_map_put(operators, "$end", end_mark);
}

static const char* skip_blank(const char* p)
{
    while(*p && isblank(*p))
        p++;
    return p;
}

static bool is_operator(const char* p)
{
    return (*p == '=' || *p == '&' || *p == '|' || *p == '<' || *p == '>' || *p == '(' || *p == ')');
}

static tmq_str_t get_operator(const char* p)
{
    if(!*p)
        return tmq_str_new("$end");
    if(*p == '(' || *p == ')' || *p == '>' || *p == '<')
        return tmq_str_new_len(p, 1);
    return tmq_str_new_len(p, 2);
}

static tmq_value_expr_type get_value_type(char* name)
{
    if(tmq_str_equal(name, "qos")) return EXPR_VALUE_QOS;
    else if(tmq_str_equal(name, "client_id")) return EXPR_VALUE_CLIENT_ID;
    else if(tmq_str_equal(name, "username")) return EXPR_VALUE_USERNAME;
    else return EXPR_VALUE_INVALID;
}

tmq_filter_expr_t* parse_filter_expression(const char* expr)
{
    tmq_vec(tmq_filter_expr_t*) operand_stack = tmq_vec_make(tmq_filter_expr_t*);
    tmq_vec(tmq_filter_expr_t*) operator_stack = tmq_vec_make(tmq_filter_expr_t*);
    const char* ptr = expr;
    while(ptr)
    {
        ptr = skip_blank(ptr);
        if(!(*ptr) || is_operator(ptr))
        {
            tmq_str_t op_name = get_operator(ptr);
            operator_into_t* operator = tmq_map_get(operators, op_name);
            if(!operator)
            {
                break;
            }
            if(!tmq_vec_empty(operator_stack))
            {
                tmq_filter_binary_expr_t** operator_stack_top =
                        (tmq_filter_binary_expr_t**)tmq_vec_at(operator_stack, tmq_vec_size(operator_stack) - 1);
                while(operator->priority <= (*operator_stack_top)->op.priority)
                {
                    tmq_filter_expr_t* right_expr = *tmq_vec_pop_back(operand_stack);
                    tmq_filter_expr_t* left_expr = *tmq_vec_pop_back(operand_stack);
                    tmq_filter_binary_expr_t* binary_expr = *(tmq_filter_binary_expr_t**)(tmq_vec_pop_back(operator_stack));
                    binary_expr->right = right_expr;
                    binary_expr->left = left_expr;
                    tmq_vec_push_back(operand_stack, (tmq_filter_expr_t*)binary_expr);
                }
            }
            tmq_filter_expr_t* operator_expr = tmq_binary_expr_new(operator->op, operator->priority);
            tmq_vec_push_back(operator_stack, operator_expr);
            ptr += tmq_str_len(op_name);
            tmq_str_free(op_name);
        }
        else
        {
            tmq_filter_expr_t* operand_expr = NULL;
            const char* p = ptr;
            while(*p && !isblank(*p) && !is_operator(p))
                p++;
            tmq_str_t value = tmq_str_new_len(ptr, p - ptr);
            tmq_value_expr_type value_type;
            if(tmq_str_startswith(value, "payload."))
                operand_expr = tmq_value_expr_new(EXPR_VALUE_PAYLOAD, value + 8);
            else if((value_type = get_value_type(value)) != EXPR_VALUE_INVALID)
                operand_expr = tmq_value_expr_new(value_type, NULL);
            else
                operand_expr = tmq_const_expr_new(value);
            tmq_str_free(value);
            tmq_vec_push_back(operand_stack, operand_expr);
            ptr = p;
        }
    }
    tmq_filter_expr_t* root = *tmq_vec_pop_back(operand_stack);
    tmq_vec_free(operand_stack);
    tmq_vec_free(operator_stack);
    return root;
}