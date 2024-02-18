//
// Created by zr on 23-4-9.
//
#include "tlog.h"
#include "rule_engine/adaptors/mqtt_delay_message_adaptor.h"
#include "mqtt/mqtt_broker.h"
#include "base/mqtt_cmd.h"
#include <stdio.h>
#include <dlfcn.h>

extern void mqtt_delay_message(void* broker_, tmq_str_t payload);
static tmq_str_t delay_message_routing_key = NULL;

tmq_adaptor_t* init_plugin(tmq_config_t* cfg, tmq_broker_t* broker, char* so_name, void* handle, char* name)
{
    char init_sym_name[100] = {0};
    sprintf(init_sym_name, "get_%s_adaptor", name);
    adaptor_getter_f init = dlsym(handle, init_sym_name);
    if(!init)
    {
        tlog_error("initializer %s not found in %s", init_sym_name, so_name);
        return NULL;
    }
    tmq_str_t error = NULL;
    tmq_adaptor_t* adaptor = NULL;
    if(tmq_str_equal(name, "delay_message"))
    {
        tmq_str_t routing_key = get_uuid();
        adaptor = init(cfg, routing_key, &error);
        if(adaptor)
        {
            ((delay_message_adaptor*)adaptor)->message_cb = mqtt_delay_message;
            ((delay_message_adaptor*)adaptor)->broker = broker;
            delay_message_routing_key = routing_key;
        }
    }
    else
        adaptor = init(cfg, NULL, &error);
    if(!adaptor)
    {
        if(error)
        {
            tlog_info("initialize %s adaptor failed: %s", name, error);
            tmq_str_free(error);
        }
        return NULL;
    }
    return adaptor;
}

int main(int argc, char* argv[])
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);
    tmq_broker_t broker;
    printf("   __                            __  ___   ____   ______  ______\n"
           "  / /_   (_)   ____    __  __   /  |/  /  / __ \\ /_  __/ /_  __/\n"
           " / __/  / /   / __ \\  / / / /  / /|_/ /  / / / /  / /     / /   \n"
           "/ /__  / /   / / / / / /_/ /  / /  / /  / /_/ /  / /     / /    \n"
           "\\__/  /_/   /_/ /_/  \\___ /  /_/  /_/   \\___\\_\\ /_/     /_/     \n"
           "                    /____/                                      \n");
    tmq_cmd_t cmd;
    tmq_cmd_init(&cmd);
    tmq_cmd_add_number(&cmd, "port", "p", "server port", 0, 1883);
    tmq_cmd_add_number(&cmd, "ssl-port", "s", "mqtts port", 0, 8883);
    tmq_cmd_add_number(&cmd, "cluster-port", "P", "cluster port", 0, 11883);
    tmq_cmd_add_string(&cmd, "config", "c", "config file path", 0, "tinymqtt.conf");
    if(tmq_cmd_parse(&cmd, argc, argv) < 0)
    {
        tmq_cmd_destroy(&cmd);
        tlog_exit();
        return 0;
    }
    /* read tinymqtt configure file */
    tmq_str_t config_path = tmq_cmd_get_string(&cmd, "config");
    tmq_config_t cfg;
    if(tmq_config_init(&cfg, config_path, "=") == 0)
        tlog_info("read config file %s ok", config_path);
    else
    {
        tlog_error("read config file error");
        return -1;
    }
    tmq_str_free(config_path);

    tmq_plugin_info_map plugins;
    tmq_map_str_init(&plugins, tmq_plugin_handle_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_str_t plugins_conf_ = tmq_config_get(&cfg, "plugins");
    if(plugins_conf_)
    {
        if(tmq_str_len(plugins_conf_) <= 2 ||
        tmq_str_at(plugins_conf_, 0) != '[' ||
        tmq_str_at(plugins_conf_, tmq_str_len(plugins_conf_) - 1) != ']')
            tlog_info("invalid plugin configuration: %s", plugins_conf_);
        else
        {
            tlog_info("loading mqtt adaptor plugins");
            tmq_str_t plugins_conf = tmq_str_substr(plugins_conf_, 1,  tmq_str_len(plugins_conf_) - 2);
            str_vec plugin_names = tmq_str_split(plugins_conf, ",");
            for(tmq_str_t* name = tmq_vec_begin(plugin_names); name != tmq_vec_end(plugin_names); name++)
            {
                tmq_str_trim(*name);
                char so_name[50] = {0};
                sprintf(so_name, "lib%s_plugin.so", *name);
                void* handle = dlopen(so_name, RTLD_LAZY);
                if(!handle)
                {
                    tlog_warn("%s not found", so_name);
                    continue;
                }
                tlog_info("load %s success", so_name);

                tmq_adaptor_t* adaptor = init_plugin(&cfg, &broker, so_name, handle, *name);
                if(!adaptor)
                    continue;

                tmq_plugin_handle_t plugin_handle = {
                        .adaptor = adaptor,
                        .so_handle = handle
                };
                tmq_map_str_init(&plugin_handle.adaptor_parameters, adaptor_value_type,
                                 MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
                tmq_map_put(plugins, *name, plugin_handle);
            }
            for(tmq_str_t* name = tmq_vec_begin(plugin_names); name != tmq_vec_end(plugin_names); name++)
                tmq_str_free(*name);
            tmq_vec_free(plugin_names);
            tmq_str_free(plugins_conf);
        }
        tmq_str_free(plugins_conf_);
    }
    if(tmq_broker_init(&broker, &cfg, &cmd, &plugins) == 0)
    {
        tmq_str_t delay_message_enable = tmq_config_get(&cfg, "delay_message_enable");
        if(delay_message_enable && tmq_str_equal(delay_message_enable, "true"))
        {
            char select_rule[256] = {0};
            sprintf(select_rule, "SELECT "
                                 "'delay-message-exchange' AS {delay_message.exchange}, "
                                 "'%s' AS {delay_message.routingKey}, "
                                 "payload.delay AS {delay_message.delayMS},"
                                 "payload.topic, payload.message, "
                                 "client_id, username, qos, retain "
                                 "FROM "
                                 "$delay", delay_message_routing_key);
            tmq_rule_engine_add_rule(&broker.rule_engine, select_rule);
        }
        tmq_str_free(delay_message_enable);

        tmq_broker_run(&broker);
    }
    tmq_map_free(plugins);
    tlog_exit();
    return 0;
}