//
// Created by just do it on 2024/1/19.
//

#ifndef TINYMQTT_MQTT_CODEC_H
#define TINYMQTT_MQTT_CODEC_H
#include "base/mqtt_vec.h"
#include <stdint.h>

typedef struct tmq_codec_s tmq_codec_t;
typedef struct tmq_tcp_conn_s tmq_tcp_conn_t;
typedef struct tmq_buffer_s tmq_buffer_t;

typedef void (*tcp_message_decoder_f) (tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer);
typedef enum tmq_codec_type_e {CLIENT_CODEC, SERVER_CODEC} tmq_codec_type;

#define CODEC_PUBLIC_MEMBERS                \
tcp_message_decoder_f decode_tcp_message;   \
tmq_codec_type type;

typedef struct tmq_codec_s {CODEC_PUBLIC_MEMBERS} tmq_codec_t;
typedef tmq_vec(uint8_t) packet_buf;

#endif //TINYMQTT_MQTT_CODEC_H
