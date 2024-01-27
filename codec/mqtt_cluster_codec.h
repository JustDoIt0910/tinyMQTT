//
// Created by just do it on 2024/1/25.
//

#ifndef TINYMQTT_MQTT_CLUSTER_CODEC_H
#define TINYMQTT_MQTT_CLUSTER_CODEC_H
#include "mqtt_codec.h"

typedef struct tmq_broker_s tmq_broker_t;
typedef char* tmq_str_t;
typedef void(*add_route_message_cb)(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_str_t topic_filter);

typedef struct tmq_cluster_codec_s
{
    LEN_BASED_CODEC_PUBLIC_MEMBERS
    add_route_message_cb on_add_route;
} tmq_cluster_codec_t;

void tmq_cluster_codec_init(tmq_cluster_codec_t* codec);
void send_add_route_message(tmq_tcp_conn_t* conn, const char* topic_filter);

#endif //TINYMQTT_MQTT_CLUSTER_CODEC_H
