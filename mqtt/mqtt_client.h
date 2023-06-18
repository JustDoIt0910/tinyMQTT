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

typedef struct tmq_client_s
{
    tmq_event_loop_t loop;
    tmq_connector_t connector;
    tmq_tcp_conn_t* conn;
    tmq_session_t* session;
    tmq_codec_t codec;
    int connect_res;
} tiny_mqtt;

typedef struct connect_options
{
    char* username, *password, *client_id;
    int clean_session;
    uint16_t keep_alive;
    char* will_message, *will_topic;
    uint8_t will_qos;
    int will_retain;
} connect_options;

tiny_mqtt* tiny_mqtt_new(const char* ip, uint16_t port);
int tiny_mqtt_connect(tiny_mqtt* mqtt, connect_options options);
void tiny_mqtt_loop(tiny_mqtt* mqtt);

#endif //TINYMQTT_MQTT_CLIENT_H
