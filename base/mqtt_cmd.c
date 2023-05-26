//
// Created by zr on 23-5-26.
//
#include "mqtt_cmd.h"

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
    tmq_map_put(cmd->short_name_index, name, tmq_vec_size(cmd->options) - 1);
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

void tmq_cmd_parse(tmq_cmd_t* cmd, int argc, char** argv)
{

}