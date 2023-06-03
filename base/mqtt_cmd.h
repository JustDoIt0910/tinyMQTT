//
// Created by zr on 23-5-26.
//

#ifndef TINYMQTT_MQTT_CMD_H
#define TINYMQTT_MQTT_CMD_H
#include "mqtt_str.h"
#include "mqtt_vec.h"
#include "mqtt_map.h"

typedef enum
{
    VALUE_TYPE_STRING,
    VALUE_TYPE_NUMBER,
    VALUE_TYPE_BOOL
} value_type_e;
typedef struct option
{
    tmq_str_t name;
    tmq_str_t short_name;
    tmq_str_t desc;
    tmq_str_t default_val;
    tmq_str_t value;
    int required;
    int present;
    value_type_e type;
} option;

typedef struct tmq_cmd_s
{
    tmq_map(const char*, int) name_index;
    tmq_map(const char*, int) short_name_index;
    tmq_vec(option) options;
} tmq_cmd_t;

void tmq_cmd_init(tmq_cmd_t* cmd);
void tmq_cmd_add_string(tmq_cmd_t* cmd, const char* name, const char* short_name,
                        const char* desc, int required, const char* default_val);
void tmq_cmd_add_number(tmq_cmd_t* cmd, const char* name, const char* short_name,
                        const char* desc, int required, int64_t default_val);
void tmq_cmd_add_bool(tmq_cmd_t* cmd, const char* name, const char* short_name, const char* desc);
int tmq_cmd_parse(tmq_cmd_t* cmd, int argc, char** argv);
int tmq_cmd_exist(tmq_cmd_t* cmd, const char* name);
tmq_str_t tmq_cmd_get_string(tmq_cmd_t* cmd, const char* name);
int64_t tmq_cmd_get_number(tmq_cmd_t* cmd, const char* name);
void tmq_cmd_destroy(tmq_cmd_t* cmd);

#endif //TINYMQTT_MQTT_CMD_H
