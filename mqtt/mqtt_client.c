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
    tmq_connect_pkt pkt;
    bzero(&pkt, sizeof(tmq_connect_pkt));
    if(mqtt->connect_ops.clean_session) pkt.flags |= 0x02;
    if(mqtt->connect_ops.will_message)
    {
        pkt.flags |= 0x04;
        pkt.flags |= (mqtt->connect_ops.will_qos << 3);
        if(mqtt->connect_ops.will_retain) pkt.flags |= 0x20;
        pkt.will_message = tmq_str_new(mqtt->connect_ops.will_message);
        pkt.will_topic = tmq_str_new(mqtt->connect_ops.will_topic);
    }
    if(mqtt->connect_ops.username)
    {
        pkt.flags |= 0x80;
        pkt.username = tmq_str_new(mqtt->connect_ops.username);
    }
    if(mqtt->connect_ops.password)
    {
        pkt.flags |= 0x40;
        pkt.password = tmq_str_new(mqtt->connect_ops.password);
    }
    pkt.client_id = tmq_str_new(mqtt->connect_ops.client_id);
    pkt.keep_alive = mqtt->connect_ops.keep_alive;
    send_connect_packet(mqtt->conn, &pkt);
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

int tiny_mqtt_connect(tiny_mqtt* mqtt, connect_options* options)
{
    mqtt->connect_ops = *options;
    tmq_connector_connect(&mqtt->connector);
    tmq_event_loop_run(&mqtt->loop);
    return mqtt->connect_res;
}

void tiny_mqtt_loop(tiny_mqtt* mqtt){tmq_event_loop_run(&mqtt->loop);}