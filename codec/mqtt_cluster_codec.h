//
// Created by just do it on 2024/1/25.
//

#ifndef TINYMQTT_MQTT_CLUSTER_CODEC_H
#define TINYMQTT_MQTT_CLUSTER_CODEC_H
#include "mqtt_codec.h"
#include "mqtt/mqtt_types.h"

typedef struct tmq_broker_s tmq_broker_t;
typedef char* tmq_str_t;
typedef void(*add_route_message_cb)(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_str_t topic_filter);
typedef void(*tun_publish_cb)(tmq_broker_t* broker, tmq_publish_pkt* publish_pkt);

typedef enum cluster_message_type_e
{
    CLUSTER_ROUTE_SYNC,
    CLUSTER_ROUTE_DEL,
    CLUSTER_TUN_PUBLISH
} cluster_message_type;

typedef struct tmq_cluster_codec_s
{
    LEN_BASED_CODEC_PUBLIC_MEMBERS
    tmq_broker_t* broker;
    tmq_mqtt_codec_t* mqtt_codec;
    add_route_message_cb on_add_route;
    tun_publish_cb on_tun_publish;
} tmq_cluster_codec_t;

void tmq_cluster_codec_init(tmq_cluster_codec_t* codec, tmq_mqtt_codec_t* mqtt_codec, tmq_broker_t* broker);
void send_route_sync_del_message(tmq_tcp_conn_t* conn, cluster_message_type type, tmq_str_t payload);
void send_publish_tun_message(tmq_tcp_conn_t* conn, tmq_str_t topic, mqtt_message* message, uint8_t retain);

#endif //TINYMQTT_MQTT_CLUSTER_CODEC_H
