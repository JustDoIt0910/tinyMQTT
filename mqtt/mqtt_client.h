//
// Created by zr on 23-4-9.
//

#ifndef TINYMQTT_MQTT_CLIENT_H
#define TINYMQTT_MQTT_CLIENT_H
#include "net/mqtt_tcp_conn.h"
#include "net/mqtt_connector.h"
#include "mqtt_session.h"
#include "mqtt_codec.h"

#define NETWORK_ERROR -1

typedef struct connect_options
{
    char* username, *password, *client_id;
    int clean_session;
    uint16_t keep_alive;
    char* will_message, *will_topic;
    uint8_t will_qos;
    int will_retain;
} connect_options;

typedef void(*mqtt_message_cb)(char* topic, char* message, uint8_t qos, uint8_t retain);
typedef struct tmq_client_s
{
    tmq_event_loop_t loop;
    tmq_tcp_conn_t* conn;
    tmq_session_t* session;
    tmq_codec_t codec;

    tmq_connector_t connector;
    connect_options connect_ops;
    int connect_res;
    sub_return_codes subscribe_res;
    mqtt_message_cb on_message;
} tiny_mqtt;

tiny_mqtt* tinymqtt_new(const char* ip, uint16_t port);
int tinymqtt_connect(tiny_mqtt* mqtt, connect_options* options);
int tinymqtt_subscribe(tiny_mqtt* mqtt, const char* topic_filter, uint8_t qos);
void tinymqtt_unsubscribe(tiny_mqtt* mqtt, const char* topic_filter);
void tinymqtt_set_message_callback(tiny_mqtt* mqtt, mqtt_message_cb cb);
void tinymqtt_publish(tiny_mqtt* mqtt, const char* topic, const char* message, uint8_t qos, int retain);
void tinymqtt_loop(tiny_mqtt* mqtt);

#endif //TINYMQTT_MQTT_CLIENT_H
