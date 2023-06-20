//
// Created by zr on 23-6-20.
//
#include "mqtt/mqtt_client.h"
#include "tlog.h"
#include <stdio.h>

void on_message(char* topic, char* message, uint8_t qos, uint8_t retain)
{
    tlog_info("received message [%s] topic=%s, qos=%u, retain=%u", message, topic, qos, retain);
}

void on_connect(tiny_mqtt* mqtt, int return_code)
{
    if(return_code == CONNECTION_ACCEPTED)
    {
        tlog_info("CONNECTION_ACCEPTED");
        //tinymqtt_publish(mqtt, "test/pub", "hello!", 2, 0);
        tinymqtt_subscribe(mqtt, "test/sub", 2);
    }
    else
        tlog_error("connect falied. return code=%d", return_code);
}

void on_disconnect(tiny_mqtt* mqtt)
{
    tlog_info("disconnected");
    tinymqtt_quit(mqtt);
}

void on_publish_finished(tiny_mqtt* mqtt, uint16_t packet_id, uint8_t qos)
{
    tlog_info("publish[id=%u, qos=%u] finished", packet_id, qos);
    tinymqtt_disconnect(mqtt);
}

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tiny_mqtt* mqtt = tinymqtt_new("192.168.3.7", 1883);
    connect_options ops = {
            "username",
            "password",
            "tmq_client_test_client",
            1,
            60,
            NULL
    };

    tinymqtt_set_message_callback(mqtt, on_message);
    tinymqtt_set_connect_callback(mqtt, on_connect);
    tinymqtt_set_disconnect_callback(mqtt, on_disconnect);
    tinymqtt_set_publish_callback(mqtt, on_publish_finished);

    tinymqtt_loop_threaded(mqtt);

    tinymqtt_connect(mqtt, &ops);

    tinymqtt_async_wait(mqtt);
    tinymqtt_destroy(mqtt);

    tlog_exit();
    return 0;
}