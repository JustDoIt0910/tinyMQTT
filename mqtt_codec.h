//
// Created by zr on 23-4-20.
//

#ifndef TINYMQTT_MQTT_CODEC_H
#define TINYMQTT_MQTT_CODEC_H
#include "mqtt_packet.h"

typedef struct tmq_tcp_conn_s tmq_tcp_conn_t;
typedef struct tmq_buffer_s tmq_buffer_t;
typedef struct tmq_broker_s tmq_broker_t;
typedef struct tmq_codec_interface_s tmq_codec_interface_t;

typedef void (*codec_message_cb)(tmq_codec_interface_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer);

typedef struct tmq_codec_interface_s
{
    codec_message_cb on_message;
} tmq_codec_interface_t;

typedef void (*tmq_on_connect_cb)(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_connect_pkt pkt);

typedef struct connect_pkt_codec
{
    codec_message_cb on_message;
    tmq_broker_t* broker;
    tmq_on_connect_cb tmq_on_connect;
} connect_pkt_codec;

void connect_msg_codec_init(connect_pkt_codec* codec, tmq_broker_t* broker, tmq_on_connect_cb on_connect);

#endif //TINYMQTT_MQTT_CODEC_H
