//
// Created by zr on 23-5-26.
//
#include "base/mqtt_config.h"
#include "tlog.h"

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tmq_config_t cfg;
    tmq_config_init(&cfg, "../../test/test.conf", "=");

    tmq_config_del(&cfg, "def");
    tmq_config_mod(&cfg, "hello", "world!");
    tmq_config_add(&cfg, "test_add", "ok");
    tmq_config_sync(&cfg);

    tlog_exit();
}