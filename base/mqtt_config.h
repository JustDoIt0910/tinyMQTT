//
// Created by zr on 23-4-30.
//

#ifndef TINYMQTT_MQTT_CONFIG_H
#define TINYMQTT_MQTT_CONFIG_H
#include <stdio.h>
#include "mqtt_map.h"
#include "mqtt_str.h"

typedef tmq_map(char*, tmq_str_t) config_map;
typedef struct tmq_config_s
{
    FILE* fp;
    config_map cfg;
    config_map cfg_add;
} tmq_config_t;

int tmq_config_init(tmq_config_t* cfg, const char* filename);
tmq_str_t tmq_config_get(tmq_config_t* cfg, const char* key);
void tmq_config_add(tmq_config_t* cfg, const char* key, const char* value);
void tmq_config_sync(tmq_config_t* cfg);
void tmq_config_reload(tmq_config_t* cfg);
void tmq_config_destroy(tmq_config_t* cfg);

#endif //TINYMQTT_MQTT_CONFIG_H
