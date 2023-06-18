//
// Created by zr on 23-4-9.
//
#include "mqtt_client.h"
#include <stdlib.h>
#include <string.h>

static void on_tcp_connected(void* arg, tmq_socket_t sock)
{
    tiny_mqtt* mqtt = arg;
    mqtt->conn = tmq_tcp_conn_new(&mqtt->loop, NULL, sock, &mqtt->codec);
}

static void on_tcp_connect_failed(void* arg)
{
    tiny_mqtt* mqtt = arg;
    mqtt->connect_res = NETWORK_ERROR;
    tmq_event_loop_quit(&mqtt->loop);
}

tiny_mqtt* tiny_mqtt_new(const char* ip, uint16_t port)
{
    tiny_mqtt* mqtt = malloc(sizeof(tiny_mqtt));
    if(!mqtt) return NULL;
    bzero(mqtt, sizeof(tiny_mqtt));
    tmq_event_loop_init(&mqtt->loop);
    tmq_codec_init(&mqtt->codec, CLIENT_CODEC);
    tmq_connector_init(&mqtt->connector, &mqtt->loop, ip, port, on_tcp_connected, on_tcp_connect_failed, mqtt, 3);
    return mqtt;
}

int tiny_mqtt_connect(tiny_mqtt* mqtt, connect_options options)
{
    tmq_connector_connect(&mqtt->connector);
    tmq_event_loop_run(&mqtt->loop);
    return mqtt->connect_res;
}

void tiny_mqtt_loop(tiny_mqtt* mqtt){tmq_event_loop_run(&mqtt->loop);}