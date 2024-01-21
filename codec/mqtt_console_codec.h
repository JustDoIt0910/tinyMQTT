//
// Created by just do it on 2024/1/18.
//

#ifndef TINYMQTT_MQTT_CONSOLE_CODEC_H
#define TINYMQTT_MQTT_CONSOLE_CODEC_H
#include "mqtt_codec.h"
#include <stdint.h>

#define CONSOLE_HEADER_SIZE 3

typedef struct tmq_broker_s tmq_broker_t;
typedef void(*add_user_message_cb)(tmq_broker_t* broker, const char* username, const char* password);

typedef enum console_parsing_state_e {PARSING_HEADER, PARSING_PAYLOAD} console_parsing_state;
typedef enum console_message_type_e
{
    ADD_USER
} console_message_type;
typedef struct console_parsing_ctx_s
{
    console_parsing_state state;
    console_message_type message_type;
    uint16_t payload_len;
} console_parsing_ctx_t;

typedef struct tmq_console_codec_s
{
    CODEC_PUBLIC_MEMBERS;
    add_user_message_cb on_add_user;
} tmq_console_codec_t;

void tmq_console_codec_init(tmq_console_codec_t* codec);
void send_add_user_message(int fd, const char* username, const char* password);

#endif //TINYMQTT_MQTT_CONSOLE_CODEC_H
