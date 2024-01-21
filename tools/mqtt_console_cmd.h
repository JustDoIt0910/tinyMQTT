//
// Created by just do it on 2024/1/20.
//

#ifndef TINYMQTT_MQTT_CONSOLE_CMD_H
#define TINYMQTT_MQTT_CONSOLE_CMD_H
#include "base/mqtt_vec.h"
#include "base/mqtt_map.h"
#include "base/mqtt_str.h"

typedef struct tmq_console_cmd_node_s tmq_console_cmd_node_t;
typedef tmq_map(char*, tmq_console_cmd_node_t*) node_map;
typedef tmq_map(char*, char*) args_map;
typedef void (*command_exec_f)(args_map* args, void* context);
typedef enum node_type_e {KEYWORD, VARIABLE, OPTION} node_type;
typedef struct tmq_console_cmd_node_s
{
    node_type type;
    tmq_str_t name;
    command_exec_f command_exec;
    node_map next;
} tmq_console_cmd_node_t;
typedef struct tmq_console_cmd_s {tmq_console_cmd_node_t root;} tmq_console_cmd_t;

typedef struct option_placeholder_s
{
    uint8_t identifier;
    tmq_str_t option_name;
    str_vec options_values;
} option_placeholder_t;

typedef struct var_placeholder_s
{
    uint8_t identifier;
    tmq_str_t var_name;
} var_placeholder_t;

#define CONSOLE_OPTION(name, ...)     tmq_console_option_new(name, __VA_ARGS__, NULL)
#define CONSOLE_VARIABLE(name)  tmq_console_var_new(name)
#define CONSOLE_COMMAND(cmd, f, ...)  tmq_console_cmd_build(&cmd->root, f, __VA_ARGS__, NULL)

void tmq_console_cmd_init(tmq_console_cmd_t* cmd);
int tmq_console_cmd_parse(tmq_console_cmd_t* cmd, const char* cmd_str, void* context);
var_placeholder_t* tmq_console_var_new(const char* name);
option_placeholder_t* tmq_console_option_new(const char* name, ...);
void tmq_console_cmd_build(tmq_console_cmd_node_t* root, command_exec_f func, ...);

#endif //TINYMQTT_MQTT_CONSOLE_CMD_H
