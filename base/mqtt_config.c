//
// Created by zr on 23-4-30.
//
#include "mqtt_config.h"
#include "mqtt_util.h"
#include <string.h>

static int read_config_file(tmq_config_t* cfg)
{
    uint32_t l_num = 0;
    char buf[1024] = {0};
    str_vec split;
    int parsing_value = 0, err = 0;
    tmq_str_t line = NULL, key = NULL, value = NULL;
    while(fgets(buf, sizeof(buf), cfg->fp))
    {
        l_num++;
        line = tmq_str_assign_n(line, buf, strlen(buf) - 1);
        if(tmq_str_len(line) == 0 || tmq_str_find(line, '#') >= 0)
        {
            if(parsing_value)
            {
                tmq_map_put(cfg->cfg, key, value);
                parsing_value = 0;
            }
            continue;
        }
        split = tmq_str_split(line, "=");
        if(tmq_vec_size(split) == 1)
        {
            if(!parsing_value)
            {
                err = -1;
                tlog_error("read_config_file() error: malformed config format at line %u", l_num);
                tmq_vec_free(split);
                goto cleanup;
            }
            value = tmq_str_append_str(value, *tmq_vec_at(split, 0));
            tmq_str_trim(value);
        }
        else if(tmq_vec_size(split) == 2)
        {
            if(parsing_value)
                tmq_map_put(cfg->cfg, key, value);
            key = tmq_str_assign(key, *tmq_vec_at(split, 0));
            tmq_str_trim(key);
            parsing_value = 1;
            value = tmq_str_new(*tmq_vec_at(split, 1));
            tmq_str_trim(value);
        }
        else
        {
            err = -1;
            tlog_error("read_config_file() error: malformed config format at line %u", l_num);
            tmq_vec_free(split);
            goto cleanup;
        }
        for(tmq_str_t* it = tmq_vec_begin(split); it != tmq_vec_end(split); it++)
            tmq_str_free(*it);
        tmq_vec_free(split);
        bzero(buf,sizeof(buf));
    }
    if(parsing_value)
        tmq_map_put(cfg->cfg, key, value);
    cleanup:
    tmq_str_free(line);
    tmq_str_free(key);
    return err;
}

int tmq_config_init(tmq_config_t* cfg, const char* filename)
{
    if(!cfg || !filename) return -1;
    cfg->fp = fopen(filename, "r+");
    if(!cfg->fp)
    {
        tlog_error("fopen() error: file not exist: %s", filename);
        return -1;
    }
    tmq_map_str_init(&cfg->cfg, tmq_str_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_vec_init(&cfg->new_items, new_item);
    return read_config_file(cfg);
}

tmq_str_t tmq_config_get(tmq_config_t* cfg, const char* key)
{
    tmq_str_t value = NULL;
    tmq_str_t* it = tmq_map_get(cfg->cfg, key);
    if(it) value = tmq_str_new(*it);
    return value;
}

void tmq_config_add(tmq_config_t* cfg, const char* key, const char* value)
{
    tmq_str_t k = tmq_str_new(key);
    tmq_str_t v = tmq_str_new(value);
    new_item item = {
            .key = k,
            .value = v
    };
    tmq_vec_push_back(cfg->new_items, item);
}

void tmq_config_sync(tmq_config_t* cfg)
{
    fseek(cfg->fp, 0, SEEK_END);

    new_item* it = tmq_vec_begin(cfg->new_items);
    for(; it != tmq_vec_end(cfg->new_items); it++)
    {
        fwrite("\n", 1, 1, cfg->fp);
        tmq_str_t line = tmq_str_new((*it).key);
        line = tmq_str_append_str(line, " = ");
        line = tmq_str_append_str(line, (*it).value);
        fwrite(line, 1, tmq_str_len(line), cfg->fp);
        fwrite("\n", 1, 1, cfg->fp);

        tmq_str_free((*it).key);
        tmq_str_free((*it).value);
        tmq_str_free(line);
    }
    fflush(cfg->fp);
}

void tmq_config_reload(tmq_config_t* cfg)
{
    tmq_map_clear(cfg->cfg);
    rewind(cfg->fp);
    read_config_file(cfg);
}

void tmq_config_destroy(tmq_config_t* cfg)
{
    tmq_config_sync(cfg);
    tmq_map_iter_t it = tmq_map_iter(cfg->cfg);
    for(; tmq_map_has_next(it); tmq_map_next(cfg->cfg, it))
        tmq_str_free(*(char**)it.second);
    tmq_map_free(cfg->cfg);
    fclose(cfg->fp);
}