//
// Created by zr on 23-5-26.
//
#include "base/mqtt_config.h"
#include "tlog.h"

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tmq_config_t cfg;
    tmq_config_init(&cfg, "../../test/test.conf");

//    tmq_map_iter_t it = tmq_map_iter(cfg.cfg);
//    for(; tmq_map_has_next(it); tmq_map_next(cfg.cfg, it))
//    {
//        printf("%s = %s\n", (char*) it.first, *(char**)it.second);
//    }

    tmq_str_t v = tmq_config_get(&cfg, "port");
    if(v) printf("%s\n", v);
    tmq_str_t v2 = tmq_config_get(&cfg, "test_add");
    if(v2) printf("%s\n", v2);

    tmq_config_add(&cfg, "test_add", "ok");
    tmq_config_sync(&cfg);

    tmq_config_reload(&cfg);
    tmq_str_t v3 = tmq_config_get(&cfg, "test_add");
    if(v3) printf("%s\n", v3);

    tlog_exit();
}