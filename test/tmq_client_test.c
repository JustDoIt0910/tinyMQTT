//
// Created by zr on 23-6-18.
//
#include "mqtt/mqtt_client.h"
#include "tlog.h"
#include <stdio.h>

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tiny_mqtt* mqtt = tiny_mqtt_new("192.168.3.7", 1883);
    connect_options ops = {
        "username",
        "password",
        "tmq_client_test_client",
        0,
        60,
        NULL
    };
    int res = tiny_mqtt_connect(mqtt, &ops);
    if(res == CONNECTION_ACCEPTED)
    {
        int ret = tiny_mqtt_subscribe(mqtt, "test", 1);
        tlog_info("%d", ret);
    }

    tiny_mqtt_loop(mqtt);
    tlog_exit();
    return 0;
}