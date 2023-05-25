//
// Created by zr on 23-4-30.
//
#include "mqtt_config.h"
#include "mqtt_util.h"

static void read_config_file(tmq_config_t* cfg)
{
    
}

int tmq_config_init(tmq_config_t* cfg, const char* filename)
{
    if(!cfg || !filename) return -1;
    cfg->fp = fopen(filename, "r+");
    if(!cfg->fp)
        fatal_error("fopen() error: file not exist");
    tmq_map_str_init(&cfg->cfg, tmq_str_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_map_str_init(&cfg->cfg_add, tmq_str_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    read_config_file(cfg);
    return 0;
}

tmq_str_t tmq_config_get(tmq_config_t* cfg, const char* key)
{

}

void tmq_config_add(tmq_config_t* cfg, const char* key, const char* value)
{

}

void tmq_config_sync(tmq_config_t* cfg)
{

}

void tmq_config_reload(tmq_config_t* cfg)
{

}

void tmq_config_destroy(tmq_config_t* cfg)
{

}