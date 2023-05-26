//
// Created by zr on 23-5-26.
//
#include "mqtt_cmd.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void tmq_cmd_init(tmq_cmd_t* cmd)
{
    tmq_vec_init(&cmd->options, option);
    tmq_map_str_init(&cmd->name_index, int, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_map_str_init(&cmd->short_name_index, int, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
}

static void add_option(tmq_cmd_t* cmd, const char* name, const char* short_name,
                       const char* desc, int required, value_type_e type, tmq_str_t default_str)
{
    option op = {
            .name = tmq_str_new(name),
            .short_name = tmq_str_new(short_name),
            .desc = tmq_str_new(desc),
            .required = required,
            .type = type,
            .default_val = default_str,
            .value = NULL,
            .present = 0
    };
    tmq_vec_push_back(cmd->options, op);
    tmq_map_put(cmd->name_index, name, tmq_vec_size(cmd->options) - 1);
    tmq_map_put(cmd->short_name_index, short_name, tmq_vec_size(cmd->options) - 1);
}

void tmq_cmd_add_string(tmq_cmd_t* cmd, const char* name, const char* short_name,
                        const char* desc, int required, const char* default_val)
{
    tmq_str_t default_str = tmq_str_new(default_val);
    add_option(cmd, name, short_name, desc, required, VALUE_TYPE_STRING, default_str);
}

void tmq_cmd_add_number(tmq_cmd_t* cmd, const char* name, const char* short_name,
                        const char* desc, int required, int64_t default_val)
{
    tmq_str_t default_str = tmq_str_parse_int(default_val, 10);
    add_option(cmd, name, short_name, desc, required, VALUE_TYPE_NUMBER, default_str);
}

void tmq_cmd_add_bool(tmq_cmd_t* cmd, const char* name, const char* short_name, const char* desc)
{
    add_option(cmd, name, short_name, desc, 0, VALUE_TYPE_BOOL, NULL);
}

static void print_usage(tmq_cmd_t* cmd, const char* prog_name)
{
    tmq_str_t line = tmq_str_new("usage: ");
    line = tmq_str_append_str(line, prog_name);
    for(option* op = tmq_vec_begin(cmd->options); op != tmq_vec_end(cmd->options); op++)
    {
        if(!op->required) continue;
        line = tmq_str_append_str(line, " --");
        line = tmq_str_append_str(line, op->name);
        if(op->type == VALUE_TYPE_STRING)
            line = tmq_str_append_str(line, "=string");
        else if(op->type == VALUE_TYPE_NUMBER)
            line = tmq_str_append_str(line, "=int");
    }
    line = tmq_str_append_str(line, " [options]...");
    printf("%s\noptions:\n", line);
    tmq_str_free(line);
    char buf[200]; char type_default[50];
    for(option* op = tmq_vec_begin(cmd->options); op != tmq_vec_end(cmd->options); op++)
    {
        bzero(buf, sizeof(buf));
        bzero(type_default, sizeof(type_default));
        if(op->required)
            op->type == VALUE_TYPE_STRING ? strcpy(type_default, "(string)"): strcpy(type_default, "(int)");
        else if(op->type != VALUE_TYPE_BOOL)
        {
            op->type == VALUE_TYPE_STRING ? sprintf(type_default, "(string[=%s])", op->default_val):
                sprintf(type_default, "(int[=%s])", op->default_val);
        }
        sprintf(buf, "-%s, --%s  %s %s", op->short_name, op->name, op->desc, type_default);
        printf("%s\n", buf);
    }
}

int tmq_cmd_parse(tmq_cmd_t* cmd, int argc, char** argv)
{
    int parsing_key = 1, error = 0;
    option* op = NULL;
    for(int i = 1; i < argc; i++)
    {
        if(parsing_key)
        {
            if(strlen(argv[i]) < 2 || argv[i][0] != '-')
            {
                error = 1;
                break;
            }
            char* key = argv[i] + 1;
            int sn = 1;
            if(*key == '-')
            {
                key++;
                sn = 0;
            }
            if(!strlen(key))
            {
                error = 1;
                break;
            }
            if(!strcmp(key, "?") || !strcmp(key, "help"))
            {
                print_usage(cmd, argv[0]);
                return -1;
            }
            int* index = sn ? tmq_map_get(cmd->short_name_index, key):
                    tmq_map_get(cmd->name_index, key);
            if(!index)
            {
                error = 1;
                break;
            }
            op = tmq_vec_at(cmd->options, *index);
            assert(op != NULL);
            op->present = 1;
            parsing_key = op->type == VALUE_TYPE_BOOL ? 1: 0;
        }
        else
        {
            op->value = tmq_str_new(argv[i]);
            parsing_key = 1;
        }
    }
    if(error)
    {
        printf("invalid arguments\n");
        print_usage(cmd, argv[0]);
        return -1;
    }
    for(op = tmq_vec_begin(cmd->options); op != tmq_vec_end(cmd->options); op++)
    {
        if(op->required && !op->present)
        {
            printf("argument --%s is required\n", op->name);
            print_usage(cmd, argv[0]);
            return -1;
        }
    }
    return 0;
}

int tmq_cmd_exist(tmq_cmd_t* cmd, const char* name)
{
    int* index = tmq_map_get(cmd->name_index, name);
    if(!index) return 0;
    option* op = tmq_vec_at(cmd->options, *index);
    return op->type == VALUE_TYPE_BOOL ? op->present: (op->value != NULL);
}

tmq_str_t tmq_cmd_get_string(tmq_cmd_t* cmd, const char* name)
{
    int* index = tmq_map_get(cmd->name_index, name);
    if(!index) return NULL;
    option* op = tmq_vec_at(cmd->options, *index);
    if(op->value)
        return tmq_str_new(op->value);
    else return tmq_str_new(op->default_val);
}

int64_t tmq_cmd_get_number(tmq_cmd_t* cmd, const char* name)
{
    int* index = tmq_map_get(cmd->name_index, name);
    if(!index) return 0;
    option* op = tmq_vec_at(cmd->options, *index);
    if(op->value)
        return strtoll(op->value, NULL, 10);
    else return strtoll(op->default_val, NULL, 10);
}

void tmq_cmd_destroy(tmq_cmd_t* cmd)
{
    for(option* op = tmq_vec_begin(cmd->options); op != tmq_vec_end(cmd->options); op++)
    {
        tmq_str_free(op->name);
        tmq_str_free(op->short_name);
        tmq_str_free(op->desc);
        tmq_str_free(op->default_val);
        tmq_str_free(op->value);
    }
    tmq_vec_free(cmd->options);
    tmq_map_free(cmd->name_index);
    tmq_map_free(cmd->short_name_index);
}