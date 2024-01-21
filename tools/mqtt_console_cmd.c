//
// Created by just do it on 2024/1/20.
//
#include "mqtt_console_cmd.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <strings.h>

void tmq_console_cmd_init(tmq_console_cmd_t* cmd)
{
    bzero(&cmd->root, sizeof(tmq_console_cmd_node_t));
    tmq_map_str_init(&cmd->root.next, tmq_console_cmd_node_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
}

option_placeholder_t* tmq_console_option_new(const char* name, ...)
{
    option_placeholder_t* option = malloc(sizeof(option_placeholder_t));
    bzero(option, sizeof(option_placeholder_t));
    option->identifier = 254;
    option->option_name = tmq_str_new(name);
    tmq_vec_init(&option->options_values, tmq_str_t);
    va_list va;
    va_start(va, name);
    const char* arg = va_arg(va,  const char*);
    while(arg)
    {
        tmq_vec_push_back(option->options_values, tmq_str_new(arg));
        arg = va_arg(va,  const char*);
    }
    va_end(va);
    return option;
}

var_placeholder_t* tmq_console_var_new(const char* name)
{
    var_placeholder_t* var = malloc(sizeof(var_placeholder_t));
    bzero(var, sizeof(var_placeholder_t));
    var->identifier = 255;
    var->var_name = tmq_str_new(name);
    return var;
}

typedef tmq_vec(tmq_console_cmd_node_t*) node_list;

static tmq_console_cmd_node_t* tmq_console_cmd_node_new(char* name, node_type type)
{
    tmq_console_cmd_node_t* node = malloc(sizeof(tmq_console_cmd_node_t));
    bzero(node, sizeof(tmq_console_cmd_node_t));
    node->type = type;
    if(name)
        node->name = tmq_str_new(name);
    tmq_map_str_init(&node->next, tmq_console_cmd_node_t*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    return node;
}

static void add_option(node_list* parents, option_placeholder_t* option)
{
    node_list curs = tmq_vec_make(tmq_console_cmd_node_t*);
    for(tmq_str_t* op = tmq_vec_begin(option->options_values); op != tmq_vec_end(option->options_values); op++)
    {
        tmq_console_cmd_node_t* node = NULL;
        tmq_console_cmd_node_t** parent = tmq_vec_begin(*parents);
        for(; parent != tmq_vec_end(*parents); parent++)
        {
            tmq_console_cmd_node_t** exist = tmq_map_get((*parent)->next, *op);
            if(exist)
            {
                node = *exist;
                break;
            }
        }
        if(!node)
            node = tmq_console_cmd_node_new(option->option_name, OPTION);
        parent = tmq_vec_begin(*parents);
        for(; parent != tmq_vec_end(*parents); parent++)
            tmq_map_put((*parent)->next, *op, node);
        tmq_vec_push_back(curs, node);
        tmq_str_free(*op);
    }
    tmq_vec_swap(*parents, curs);
    tmq_vec_free(curs);
}

static void add_variable(node_list* parents, var_placeholder_t* var)
{
    tmq_console_cmd_node_t* node = NULL;
    tmq_console_cmd_node_t** parent = tmq_vec_begin(*parents);
    for(; parent != tmq_vec_end(*parents); parent++)
    {
        tmq_console_cmd_node_t **exist = tmq_map_get((*parent)->next, "__variable");
        if(exist)
        {
            node = *exist;
            break;
        }
    }
    if(!node)
        node = tmq_console_cmd_node_new(var->var_name, VARIABLE);
    parent = tmq_vec_begin(*parents);
    for(; parent != tmq_vec_end(*parents); parent++)
        tmq_map_put((*parent)->next, "__variable", node);
    tmq_str_free(var->var_name);
    tmq_vec_clear(*parents);
    tmq_vec_push_back(*parents, node);
}

static void add_keyword(node_list* parents, const char* keyword)
{
    tmq_console_cmd_node_t* node = NULL;
    tmq_console_cmd_node_t** parent = tmq_vec_begin(*parents);
    for(; parent != tmq_vec_end(*parents); parent++)
    {
        tmq_console_cmd_node_t **exist = tmq_map_get((*parent)->next, (char*)keyword);
        if(exist)
        {
            node = *exist;
            break;
        }
    }
    if(!node)
        node = tmq_console_cmd_node_new(NULL, KEYWORD);
    parent = tmq_vec_begin(*parents);
    for(; parent != tmq_vec_end(*parents); parent++)
        tmq_map_put((*parent)->next, (char*)keyword, node);
    tmq_vec_clear(*parents);
    tmq_vec_push_back(*parents, node);
}

void tmq_console_cmd_build(tmq_console_cmd_node_t* root, command_exec_f func, ...)
{
    va_list va;
    va_start(va, func);
    void* arg = va_arg(va, void*);
    node_list parents = tmq_vec_make(tmq_console_cmd_node_t*);
    tmq_vec_push_back(parents, root);
    while(arg)
    {
        if(*((uint8_t*)arg) == 254)
        {
            add_option(&parents, (option_placeholder_t*)arg);
            free(arg);
        }
        else if(*((uint8_t*)arg) == 255)
        {
            add_variable(&parents, (var_placeholder_t*)arg);
            free(arg);
        }
        else
            add_keyword(&parents, (const char*)arg);
        arg = va_arg(va, void*);
    }
    tmq_console_cmd_node_t** node = tmq_vec_begin(parents);
    for(; node != tmq_vec_end(parents); node++)
        (*node)->command_exec = func;
    va_end(va);
}

int tmq_console_cmd_parse(tmq_console_cmd_t* cmd, const char* cmd_str, void* context)
{
    str_vec tokens = tmq_str_split((tmq_str_t)cmd_str, " ");
    tmq_console_cmd_node_t* cur = &cmd->root;
    int ret = 0;
    args_map args;
    tmq_map_str_init(&args, char*, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    for(tmq_str_t* t = tmq_vec_begin(tokens); t != tmq_vec_end(tokens); t++)
    {
        tmq_console_cmd_node_t** next = tmq_map_get(cur->next, "__variable");
        if(!next)
            next = tmq_map_get(cur->next, *t);
        if(!next)
        {
            ret = -1;
            break;
        }
        cur = *next;
        if(cur->type != KEYWORD)
            tmq_map_put(args, cur->name, *t);
    }
    if(cur->command_exec)
        cur->command_exec(&args, context);
    else ret = -1;
    for(tmq_str_t* t = tmq_vec_begin(tokens); t != tmq_vec_end(tokens); t++)
        tmq_str_free(*t);
    tmq_vec_free(tokens);
    tmq_map_free(args);
    return ret;
}