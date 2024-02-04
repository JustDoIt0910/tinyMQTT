//
// Created by just do it on 2024/2/2.
//
#include "mqtt_rule_parser.h"
#include "base/mqtt_map.h"
#include <ctype.h>
#include <string.h>
#include <malloc.h>

static tmq_map(char*, operator_into_t) operators;

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

static tmq_filter_expr_t* parse_filter_expression(tmq_rule_parser_t* parser, const char* expr)
{
    tmq_filter_expr_t* root = NULL;
    tmq_vec(tmq_filter_expr_t*) operand_stack = tmq_vec_make(tmq_filter_expr_t*);
    tmq_vec(tmq_filter_expr_t*) operator_stack = tmq_vec_make(tmq_filter_expr_t*);
    tmq_vec(tmq_filter_expr_t*) nodes = tmq_vec_make(tmq_filter_expr_t*);
    tmq_vec(tmq_filter_expr_t*) left_parentheses_nodes = tmq_vec_make(tmq_filter_expr_t*);
    const char* ptr = expr;
    int done = 0;
    while(ptr && !done)
    {
        ptr = skip_blank(ptr);
        if(!(*ptr) || is_operator(ptr))
        {
            tmq_str_t op_name = get_operator(ptr);
            operator_into_t* operator = tmq_map_get(operators, op_name);
            if(!operator)
            {
                tmq_str_free(op_name);
                goto failed;
            }
            if(!tmq_vec_empty(operator_stack) && operator->op != EXPR_OP_LP)
            {
                tmq_filter_binary_expr_t** operator_stack_top =
                        (tmq_filter_binary_expr_t**)tmq_vec_at(operator_stack, tmq_vec_size(operator_stack) - 1);
                while(operator->priority <= (*operator_stack_top)->op.priority)
                {
                    if((*operator_stack_top)->op.op == EXPR_OP_LP)
                    {
                        tmq_vec_pop_back(operator_stack);
                        break;
                    }
                    tmq_filter_expr_t* right_expr = *tmq_vec_pop_back(operand_stack);
                    tmq_filter_expr_t* left_expr = *tmq_vec_pop_back(operand_stack);
                    tmq_filter_binary_expr_t* binary_expr = *(tmq_filter_binary_expr_t**)(tmq_vec_pop_back(operator_stack));
                    binary_expr->right = right_expr;
                    binary_expr->left = left_expr;
                    tmq_vec_push_back(operand_stack, (tmq_filter_expr_t*)binary_expr);
                    if(tmq_vec_empty(operator_stack))
                        break;
                    operator_stack_top =
                            (tmq_filter_binary_expr_t**)tmq_vec_at(operator_stack, tmq_vec_size(operator_stack) - 1);
                }
            }
            if(*ptr && *ptr != ')')
            {
                tmq_filter_expr_t* operator_expr = tmq_binary_expr_new(operator->op, operator->priority);
                tmq_vec_push_back(operator_stack, operator_expr);
                if(operator->op != EXPR_OP_LP)
                    tmq_vec_push_back(nodes, operator_expr);
                else
                    tmq_vec_push_back(left_parentheses_nodes, operator_expr);
            }
            if(*ptr) ptr += tmq_str_len(op_name);
            else done = 1;
            tmq_str_free(op_name);
        }
        else
        {
            tmq_filter_expr_t* operand_expr = NULL;
            const char* p = ptr;
            while(*p && !isblank(*p) && !is_operator(p))
                p++;
            tmq_str_t value = tmq_str_new_len(ptr, p - ptr);
            event_data_field_meta_t** meta;
            if(tmq_str_startswith(value, "payload."))
            {
                meta = tmq_map_get(parser->event_source->fields_meta, "payload");
                if(!meta)
                {
                    tmq_str_free(value);
                    goto failed;
                }
                operand_expr = tmq_value_expr_new(*meta, value + 8);
            }
            else if((meta = tmq_map_get(parser->event_source->fields_meta, value)) != NULL)
                operand_expr = tmq_value_expr_new(*meta, NULL);
            else
                operand_expr = tmq_const_expr_new(value);
            tmq_str_free(value);
            tmq_vec_push_back(operand_stack, operand_expr);
            tmq_vec_push_back(nodes, operand_expr);
            ptr = p;
        }
    }
    if(tmq_vec_size(operand_stack) != 1)
        goto failed;
    root = *tmq_vec_pop_back(operand_stack);
    goto end;
    failed:
    for(tmq_filter_expr_t** it = tmq_vec_begin(nodes); it != tmq_vec_end(nodes); it++)
        tmq_expr_free(*it);
    end:
    for(tmq_filter_expr_t** it = tmq_vec_begin(left_parentheses_nodes); it != tmq_vec_end(left_parentheses_nodes); it++)
        tmq_expr_free(*it);
    tmq_vec_free(nodes);
    tmq_vec_free(left_parentheses_nodes);
    tmq_vec_free(operand_stack);
    tmq_vec_free(operator_stack);
    return root;
}

void tmq_rule_parser_init(tmq_rule_parser_t* parser)
{
    bzero(parser, sizeof(tmq_rule_parser_t));
    tmq_map_str_init(&operators, operator_into_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    operator_into_t eq = {.op = EXPR_OP_EQ, .priority = 3};
    operator_into_t gt = {.op = EXPR_OP_GT, .priority = 3};
    operator_into_t gte = {.op = EXPR_OP_GTE, .priority = 3};
    operator_into_t lt = {.op = EXPR_OP_LT, .priority = 3};
    operator_into_t lte = {.op = EXPR_OP_LTE, .priority = 3};
    operator_into_t and = {.op = EXPR_OP_AND, .priority = 2};
    operator_into_t or = {.op = EXPR_OP_OR, .priority = 2};
    operator_into_t left_parentheses = {.op = EXPR_OP_LP, .priority = 1};
    operator_into_t right_parentheses = {.op = EXPR_OP_RP, .priority = 1};
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

extern tmq_map(char*, event_source_info_t) event_sources_g;

tmq_rule_parse_result_t* tmq_rule_parse(tmq_rule_parser_t* parser, const char* rule)
{
    if(!rule) return NULL;
    const char* ptr = skip_blank(rule);
    if(strncasecmp(ptr, "select ", 7) != 0)
        return NULL;
    ptr += 7;
    tmq_rule_parse_result_t* result = malloc(sizeof(tmq_rule_parse_result_t));
    bzero(result, sizeof(tmq_rule_parse_result_t));
    tmq_map_str_init(&result->select_schema_map, tmq_str_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    ptr = skip_blank(ptr);
    while(1)
    {
        const char* p = ptr;
        while(*p && !isblank(*p) && *p != ',')
            p++;
        tmq_str_t ori_column = tmq_str_new_len(ptr, p - ptr);
        tmq_map_put(result->select_schema_map, ori_column, ori_column);
        ptr = skip_blank(p);
        if(*ptr == ',')
        {
            ptr = skip_blank(ptr + 1);
            continue;
        }
        if(strncasecmp(ptr, "as ", 3) == 0)
        {
            ptr = skip_blank(ptr + 3);
            p = ptr;
            while(*p && !isblank(*p) && *p != ',')
                p++;
            tmq_str_t target_column = tmq_str_new_len(ptr, p - ptr);
            tmq_map_put(result->select_schema_map, ori_column, target_column);
            tmq_str_free(ori_column);
            ptr = skip_blank(p);
            if(*ptr == ',')
            {
                ptr = skip_blank(ptr + 1);
                continue;
            }
        }
        if(strncasecmp(ptr, "from ", 5) == 0)
        {
            ptr = skip_blank(ptr + 5);
            break;
        }
        goto failed;
    }
    if(*ptr == '{')
    {
        const char* p = ptr;
        while(*p && !isblank(*p) && *p != '}')
            p++;
        if(*p != '}')
            goto failed;
        tmq_str_t source = tmq_str_new_len(ptr + 1, p - ptr - 1);
        event_source_info_t* source_info = tmq_map_get(event_sources_g, source);
        if(!source_info)
        {
            tmq_str_free(source);
            goto failed;
        }
        parser->event_source = source_info;
        result->event_source = source_info->source;
        tmq_str_free(source);
        ptr = skip_blank(p + 1);
    }
    else
    {
        const char* p = ptr;
        while(*p && !isblank(*p))
            p++;
        event_source_info_t* source_info = tmq_map_get(event_sources_g, "message");
        if(!source_info)
            goto failed;
        parser->event_source = source_info;
        result->event_source = MESSAGE;
        result->source_topic = tmq_str_new_len(ptr, p - ptr);
        ptr = skip_blank(p);
    }
    if(*ptr)
    {
        if(strncasecmp(ptr, "where ", 6) != 0)
            goto failed;
        ptr = skip_blank(ptr + 6);
        result->filter = parse_filter_expression(parser, ptr);
        if(!result->filter)
            goto failed;
    }
    return result;

    failed:
    tmq_rule_parse_result_free(result);
    return NULL;
}

void tmq_rule_parse_result_free(tmq_rule_parse_result_t* result)
{
    for(tmq_map_iter_t iter = tmq_map_iter(result->select_schema_map);
        tmq_map_has_next(iter);
        tmq_map_next(result->select_schema_map, iter))
        tmq_str_free(*(tmq_str_t*)iter.second);
    tmq_str_free(result->source_topic);
    free(result);
}

void tmq_rule_parse_result_print(tmq_rule_parse_result_t* result)
{
    printf("Schema:\n");
    for(tmq_map_iter_t iter = tmq_map_iter(result->select_schema_map);
        tmq_map_has_next(iter);
        tmq_map_next(result->select_schema_map, iter))
        printf("{%s->%s}\n", (char*)(iter.first), *(char**)(iter.second));
    printf("Event Source:\n");
    if(result->event_source == DEVICE) printf("{DEVICE}\n");
    else if(result->event_source == TOPIC) printf("{TOPIC}\n");
    else if(result->event_source == SUBSCRIPTION) printf("{SUB_UNSUB}\n");
    else printf("%s\n", result->source_topic);
    printf("Expression Tree InOrder:\n");
    tmq_print_filter_inorder(result->filter);
    printf("\nExpression Tree PreOrder:\n");
    tmq_print_filter_preorder(result->filter);
    printf("\n");
}