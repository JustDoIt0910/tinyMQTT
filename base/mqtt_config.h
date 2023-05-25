//
// Created by zr on 23-4-30.
//

#ifndef TINYMQTT_MQTT_CONFIG_H
#define TINYMQTT_MQTT_CONFIG_H
#include <stdio.h>
#include "mqtt_map.h"
#include "mqtt_str.h"

typedef tmq_map(const char*, tmq_str_t) content_map;
typedef struct new_item { tmq_str_t key, value;} new_item;
typedef tmq_vec(new_item) new_item_list;

typedef struct tmq_config_s
{
    FILE* fp;
    content_map cfg;
    new_item_list new_items;
} tmq_config_t;

int tmq_config_init(tmq_config_t* cfg, const char* filename);
tmq_str_t tmq_config_get(tmq_config_t* cfg, const char* key);
void tmq_config_add(tmq_config_t* cfg, const char* key, const char* value);
void tmq_config_sync(tmq_config_t* cfg);
void tmq_config_reload(tmq_config_t* cfg);
void tmq_config_destroy(tmq_config_t* cfg);

#endif //TINYMQTT_MQTT_CONFIG_H
