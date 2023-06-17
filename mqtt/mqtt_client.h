//
// Created by zr on 23-4-9.
//

#ifndef TINYMQTT_MQTT_CLIENT_H
#define TINYMQTT_MQTT_CLIENT_H
#include "net/mqtt_tcp_conn.h"
#include "net/mqtt_connector.h"
#include "mqtt_session.h"
#include "mqtt_codec.h"

typedef struct tmq_client_s
{
    tmq_event_loop_t loop;
    tmq_connector_t connector;
    tmq_tcp_conn_t* conn;
    tmq_session_t* session;
    tmq_codec_t codec;
} tiny_mqtt;

tiny_mqtt* tiny_mqtt_new(const char* ip, uint16_t port);
void tiny_mqtt_loop(tiny_mqtt* mqtt);

#endif //TINYMQTT_MQTT_CLIENT_H
