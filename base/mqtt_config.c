//
// Created by zr on 23-4-30.
//
#include "mqtt_config.h"
#include "mqtt_util.h"
#include <string.h>

static int read_config_file(tmq_config_t* cfg)
{
    uint32_t l_num = 0;
    char buf[MAX_LINE_SIZE] = {0};
    str_vec split;
    int parsing_value = 0, err = 0;
    tmq_str_t line = NULL, key = NULL, value = NULL;
    while(fgets(buf, sizeof(buf), cfg->fp))
    {
        l_num++;
        size_t len = buf[strlen(buf) - 1] == '\n' ? strlen(buf) - 1: strlen(buf);
        line = tmq_str_assign_n(line, buf, len);
        if(tmq_str_len(line) == 0 || tmq_str_find(line, '#') >= 0)
        {
            if(parsing_value)
            {
                config_value cfg_value = {
                        .value = value,
                        .modified = 0,
                        .deleted = 0
                };
                tmq_map_put(cfg->cfg, key, cfg_value);
                parsing_value = 0;
            }
            continue;
        }
        split = tmq_str_split(line, cfg->delimeter);
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
            {
                config_value cfg_value = {
                        .value = value,
                        .modified = 0,
                        .deleted = 0
                };
                tmq_map_put(cfg->cfg, key, cfg_value);
            }
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
    {
        config_value cfg_value = {
                .value = value,
                .modified = 0,
                .deleted = 0
        };
        tmq_map_put(cfg->cfg, key, cfg_value);
    }
    cleanup:
    tmq_str_free(line);
    tmq_str_free(key);
    return err;
}

int tmq_config_init(tmq_config_t* cfg, const char* filename, const char* delimeter)
{
    if(!cfg || !filename) return -1;
    if(strlen(delimeter) > MAX_DELIMETER_SIZE)
    {
        tlog_error("delimeter too long");
        return -1;
    }
    cfg->fp = fopen(filename, "r+");
    if(!cfg->fp)
    {
        tlog_error("fopen() error: file not exist: %s", filename);
        return -1;
    }
    tmq_map_str_init(&cfg->cfg, config_value, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_vec_init(&cfg->new_items, new_item);
    cfg->mod = 0;
    bzero(cfg->delimeter, sizeof(cfg->delimeter));
    strcpy(cfg->delimeter, delimeter);
    cfg->filename = tmq_str_new(filename);
    return read_config_file(cfg);
}

tmq_str_t tmq_config_get(tmq_config_t* cfg, const char* key)
{
    tmq_str_t value = NULL;
    config_value* it = tmq_map_get(cfg->cfg, key);
    if(it && !it->deleted) value = tmq_str_new((*it).value);
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

void tmq_config_mod(tmq_config_t* cfg, const char* key, const char* value)
{
    config_value* cfg_value = tmq_map_get(cfg->cfg, key);
    if(!cfg_value) return;
    cfg_value->value = tmq_str_assign(cfg_value->value, value);
    cfg_value->modified = 1;
    cfg->mod = 1;
}

void tmq_config_del(tmq_config_t* cfg, const char* key)
{
    config_value* cfg_value = tmq_map_get(cfg->cfg, key);
    if(!cfg_value) return;
    cfg_value->deleted = 1;
    cfg->mod = 1;
}

void tmq_config_sync(tmq_config_t* cfg)
{
    if(cfg->mod)
    {
        /* if config content is modified, store the modified content to the config file */
        cfg->mod = 0;
        fseek(cfg->fp, 0, SEEK_SET);
        tmq_str_t tmp_filename = tmq_str_new(cfg->filename);
        tmp_filename = tmq_str_append_str(tmp_filename, ".tmp");
        FILE* tmp = fopen(tmp_filename, "w");

        char buf[MAX_LINE_SIZE] = {0};
        str_vec split;
        tmq_str_t line = NULL, key = NULL;
        int skip = 0;
        while(fgets(buf, sizeof(buf), cfg->fp))
        {
            line = tmq_str_assign_n(line, buf, strlen(buf));
            if((tmq_str_len(line) == 1 && tmq_str_at(line, 0) == '\n')
            || tmq_str_find(line, '#') >= 0)
            {
                fwrite(line, 1, tmq_str_len(line), tmp);
                skip = 0;
                continue;
            }
            split = tmq_str_split(line, cfg->delimeter);
            if(tmq_vec_size(split) == 1 && !skip)
                fwrite(line, 1, tmq_str_len(line), tmp);
            else if(tmq_vec_size(split) == 2)
            {
                skip = 0;
                key = tmq_str_assign(key, *tmq_vec_at(split, 0));
                tmq_str_trim(key);
                config_value* cfg_value = tmq_map_get(cfg->cfg, key);
                if(!cfg_value || (!cfg_value->deleted && !cfg_value->modified))
                    fwrite(line, 1, tmq_str_len(line), tmp);
                else if(cfg_value->deleted)
                {
                    skip = 1;
                    tmq_str_free(cfg_value->value);
                    tmq_map_erase(cfg->cfg, key);
                }
                else
                {
                    skip = 1;
                    tmq_str_t new_line = tmq_str_new(key);
                    new_line = tmq_str_append_str(new_line, cfg->delimeter);
                    new_line = tmq_str_append_str(new_line, cfg_value->value);
                    new_line = tmq_str_append_char(new_line, '\n');
                    fwrite(new_line, 1, tmq_str_len(new_line), tmp);
                    cfg_value->modified = 0;
                    tmq_str_free(new_line);
                }
            }
            for(tmq_str_t* it = tmq_vec_begin(split); it != tmq_vec_end(split); it++)
                tmq_str_free(*it);
            tmq_vec_free(split);
            bzero(buf,sizeof(buf));
        }
        tmq_str_free(line);
        tmq_str_free(key);
        fflush(tmp);
        fclose(cfg->fp);
        rename(tmp_filename, cfg->filename);
        tmq_str_free(tmp_filename);
        cfg->fp = fopen(cfg->filename, "r+");
    }
    /* if new config items has been added, append them to the config file */
    if(!tmq_vec_size(cfg->new_items))
        return;
    fseek(cfg->fp, 0, SEEK_END);
    new_item* it = tmq_vec_begin(cfg->new_items);
    for(; it != tmq_vec_end(cfg->new_items); it++)
    {
        fwrite("\n", 1, 1, cfg->fp);
        tmq_str_t line = tmq_str_new((*it).key);
        line = tmq_str_append_str(line, cfg->delimeter);
        line = tmq_str_append_str(line, (*it).value);
        fwrite(line, 1, tmq_str_len(line), cfg->fp);
        fwrite("\n", 1, 1, cfg->fp);

        tmq_str_free((*it).key);
        tmq_str_free((*it).value);
        tmq_str_free(line);
    }
    tmq_vec_clear(cfg->new_items);
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
        tmq_str_free(((config_value*) it.second)->value);
    tmq_map_free(cfg->cfg);
    tmq_vec_free(cfg->new_items);
    tmq_str_free(cfg->filename);
    fclose(cfg->fp);
}