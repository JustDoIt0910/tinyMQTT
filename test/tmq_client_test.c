//
// Created by zr on 23-6-18.
//
#include "mqtt/mqtt_client.h"
#include "tlog.h"
#include <stdio.h>

void on_message(char* topic, char* message, uint8_t qos, uint8_t retain)
{
    tlog_info("received message [%s] topic=%s, qos=%u, retain=%u", message, topic, qos, retain);
}

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tiny_mqtt* mqtt = tinymqtt_new("192.168.3.7", 1883);
    connect_options ops = {
        "username",
        "password",
        "tmq_client_test_client",
        0,
        60,
        NULL
    };
    int res = tinymqtt_connect(mqtt, &ops);
    if(res == CONNECTION_ACCEPTED)
    {
        tinymqtt_subscribe(mqtt, "test/sub", 1);
        tinymqtt_set_message_callback(mqtt, on_message);

        tinymqtt_publish(mqtt, "test/pub", "hello!", 1, 0);
    }

    tinymqtt_loop(mqtt);
    tlog_exit();
    return 0;
}