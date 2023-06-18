//
// Created by zr on 23-6-18.
//
#include "mqtt/mqtt_client.h"
#include "tlog.h"
#include <stdio.h>

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tiny_mqtt* mqtt = tiny_mqtt_new("127.0.0.1", 9999);
    connect_options ops = {};
    int res = tiny_mqtt_connect(mqtt, &ops);
    printf("%d\n", res);

    tlog_exit();
    return 0;
}