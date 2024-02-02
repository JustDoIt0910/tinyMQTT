//
// Created by just do it on 2024/1/18.
//

#ifndef TINYMQTT_MQTT_CONSOLE_CODEC_H
#define TINYMQTT_MQTT_CONSOLE_CODEC_H
#include "mqtt_codec.h"
#include <stdint.h>

typedef struct tmq_broker_s tmq_broker_t;
typedef struct tmq_io_context_s tmq_io_context_t;
typedef struct user_op_context_s user_op_context_t;
typedef void(*add_user_message_cb)(tmq_broker_t* broker, tmq_tcp_conn_t* conn, const char* username, const char* password);

typedef enum console_message_type_e
{
    ADD_USER,
    DEL_USER,
    CHANGE_PWD,
    ADD_ACL,
    LIST_ACL
} console_message_type;

typedef struct tmq_console_codec_s
{
    CODEC_PUBLIC_MEMBERS
    LEN_BASED_CODEC_PUBLIC_MEMBERS
    add_user_message_cb on_add_user;
} tmq_console_codec_t;

void tmq_console_codec_init(tmq_console_codec_t* codec);
void send_user_operation_reply(tmq_tcp_conn_t* conn, user_op_context_t* ctx);
int send_add_user_message(int fd, const char* username, const char* password);
int receive_user_operation_reply(int fd);

#endif //TINYMQTT_MQTT_CONSOLE_CODEC_H
