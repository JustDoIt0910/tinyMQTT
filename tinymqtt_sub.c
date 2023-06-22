//
// Created by zr on 23-6-20.
//
#include "mqtt/mqtt_client.h"
#include "base/mqtt_cmd.h"
#include "tlog.h"

void on_message(char* topic, char* message, uint8_t qos, uint8_t retain)
{
    tlog_info("received message [%s] topic=%s, qos=%u, retain=%u", message, topic, qos, retain);
}

int main(int argc, char* argv[])
{
    tmq_cmd_t cmd;
    tmq_cmd_init(&cmd);
    tmq_cmd_add_string(&cmd, "host", "h", "server ip address", 0, "127.0.0.1");
    tmq_cmd_add_number(&cmd, "port", "p", "server port", 0, 1883);
    tmq_cmd_add_string(&cmd, "client_id", "c", "client id", 0, "");
    tmq_cmd_add_bool(&cmd, "clean", "s", "clean session");
    tmq_cmd_add_number(&cmd, "keep_alive", "k", "keep alive", 0, 60);
    tmq_cmd_add_string(&cmd, "user", "u", "username", 0, "tinymqtt_sub");
    tmq_cmd_add_string(&cmd, "pwd", "p", "password", 0, "tinymqtt_sub");
    tmq_cmd_add_string(&cmd, "topic", "t", "topic", 1, "");
    tmq_cmd_add_number(&cmd, "qos", "q", "required qos", 0, 0);

    tmq_cmd_add_bool(&cmd, "will", "W", "has will message");
    tmq_cmd_add_string(&cmd, "will_topic", "T", "will topic", 0, "");
    tmq_cmd_add_string(&cmd, "will_message", "M", "will message", 0, "");
    tmq_cmd_add_number(&cmd, "will_qos", "Q", "will qos", 0, 0);
    tmq_cmd_add_bool(&cmd, "will_retain", "R", "will retain");

    if(tmq_cmd_parse(&cmd, argc, argv) != -1)
    {
        int clean_session = tmq_cmd_exist(&cmd, "clean");
        tmq_str_t client_id = tmq_cmd_get_string(&cmd, "client_id");
        /* if you don't provide a client id, the clean_session flag must be set to 1 */
        if(tmq_str_len(client_id) == 0)
            clean_session = 1;
        connect_options options = {
                tmq_cmd_get_string(&cmd, "user"),
                tmq_cmd_get_string(&cmd, "pwd"),
                tmq_cmd_get_string(&cmd, "client_id"),
                clean_session,
                tmq_cmd_get_number(&cmd, "keep_alive"),
                NULL,NULL, 0, 0
        };
        if(tmq_cmd_exist(&cmd, "will"))
        {
            options.will_topic = tmq_cmd_get_string(&cmd, "will_topic");
            options.will_message = tmq_cmd_get_string(&cmd, "will_message");
            options.will_qos = tmq_cmd_get_number(&cmd, "will_qos");
            options.will_retain = tmq_cmd_exist(&cmd, "will_retain");
        }
        tiny_mqtt* mqtt = tinymqtt_new(tmq_cmd_get_string(&cmd, "host"), tmq_cmd_get_number(&cmd, "port"));
        int ret = tinymqtt_connect(mqtt, &options);
        if(ret == CONNECTION_ACCEPTED)
        {
            tinymqtt_set_message_callback(mqtt, on_message);
            tinymqtt_subscribe(mqtt, tmq_cmd_get_string(&cmd, "topic"), tmq_cmd_get_number(&cmd, "qos"));
            tinymqtt_loop(mqtt);
        }
        else
            tlog_info("connect failed");
        tmq_str_free(options.client_id);
        tmq_str_free(options.username);
        tmq_str_free(options.password);
        if(options.will_topic)
            tmq_str_free(options.will_topic);
        if(options.will_message)
            tmq_str_free(options.will_message);
        tinymqtt_destroy(mqtt);
    }
    tmq_cmd_destroy(&cmd);
    return 0;
}