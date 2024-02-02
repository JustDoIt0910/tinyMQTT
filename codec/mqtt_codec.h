//
// Created by just do it on 2024/1/19.
//

#ifndef TINYMQTT_MQTT_CODEC_H
#define TINYMQTT_MQTT_CODEC_H
#include "base/mqtt_vec.h"
#include <stdint.h>

#define LEN_BASED_HEADER_SIZE 3

typedef struct tmq_codec_s tmq_codec_t;
typedef struct tmq_tcp_conn_s tmq_tcp_conn_t;
typedef struct tmq_buffer_s tmq_buffer_t;
typedef uint8_t len_based_message_type;
typedef tmq_vec(uint8_t) packet_buf;

typedef void (*tcp_message_decoder_f) (tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer);

typedef enum tmq_codec_type_e
{
    CLIENT_CODEC,
    SERVER_CODEC
} tmq_codec_type;

#define CODEC_PUBLIC_MEMBERS                \
tcp_message_decoder_f decode_tcp_message;   \
tmq_codec_type type;

typedef struct tmq_codec_s
{
    CODEC_PUBLIC_MEMBERS
} tmq_codec_t;

typedef struct tmq_len_based_codec_s tmq_len_based_codec_t;
typedef void(*payload_parser)(tmq_len_based_codec_t*, tmq_tcp_conn_t*, tmq_buffer_t*, uint16_t);

#define LEN_BASED_CODEC_PUBLIC_MEMBERS      \
int n_parsers;                              \
payload_parser* payload_parsers;

typedef struct tmq_len_based_codec_s
{
    CODEC_PUBLIC_MEMBERS
    LEN_BASED_CODEC_PUBLIC_MEMBERS
} tmq_len_based_codec_t;

void tmq_len_based_codec_init(tmq_len_based_codec_t* codec);
void tmq_len_based_codec_register_parser(tmq_len_based_codec_t* codec, len_based_message_type message_type,
                                         payload_parser parser);

typedef enum len_based_parsing_state_e
{
    PARSING_HEADER,
    PARSING_PAYLOAD
} length_based_parsing_state;

typedef struct len_based_parsing_ctx_s
{
    length_based_parsing_state state;
    len_based_message_type message_type;
    uint16_t payload_len;
} len_based_parsing_ctx_t;

#endif //TINYMQTT_MQTT_CODEC_H
