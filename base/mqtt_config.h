//
// Created by zr on 23-4-30.
//

#ifndef TINYMQTT_MQTT_CONFIG_H
#define TINYMQTT_MQTT_CONFIG_H
#include <stdio.h>
#include "mqtt_map.h"
#include "mqtt_str.h"
#define MAX_LINE_SIZE       1024
#define MAX_DELIMETER_SIZE  20

typedef struct config_value
{
    tmq_str_t value;
    int modified, deleted;
} config_value;
typedef tmq_map(const char*, config_value) content_map;
typedef struct new_item { tmq_str_t key, value;} new_item;
typedef tmq_vec(new_item) new_item_list;

typedef struct tmq_config_s
{
    FILE* fp;
    int mod;
    content_map cfg;
    new_item_list new_items;
    char delimeter[MAX_DELIMETER_SIZE + 1];
    tmq_str_t filename;
} tmq_config_t;

int tmq_config_init(tmq_config_t* cfg, const char* filename, const char* delimeter);
tmq_str_t tmq_config_get(tmq_config_t* cfg, const char* key);
int tmq_config_exist(tmq_config_t* cfg, const char* key);
void tmq_config_add(tmq_config_t* cfg, const char* key, const char* value);
void tmq_config_mod(tmq_config_t* cfg, const char* key, const char* value);
void tmq_config_del(tmq_config_t* cfg, const char* key);
void tmq_config_sync(tmq_config_t* cfg);
void tmq_config_reload(tmq_config_t* cfg);
void tmq_config_destroy(tmq_config_t* cfg);

#endif //TINYMQTT_MQTT_CONFIG_H
