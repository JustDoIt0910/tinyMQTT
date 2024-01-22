//
// Created by just do it on 2024/1/18.
//
#include "mqtt_console_codec.h"
#include "net/mqtt_tcp_conn.h"
#include "mqtt/mqtt_types.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static void parse_header(tmq_buffer_t* buffer, console_parsing_ctx_t* parsing_ctx)
{
    tmq_buffer_read(buffer, (char*)&parsing_ctx->message_type, 1);
    tmq_buffer_read16(buffer, &parsing_ctx->payload_len);
}

static void parse_add_user_message(tmq_console_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer, uint16_t len)
{
    char payload[128] = {0};
    tmq_buffer_read(buffer, payload, len);
    console_conn_ctx_t* ctx = conn->context;
    const char* user = payload;
    size_t user_len = strlen(user);
    const char* pwd = payload + user_len + 1;
    codec->on_add_user(ctx->broker, conn, user, pwd);
}

static void(*payload_parsers[])(tmq_console_codec_t*, tmq_tcp_conn_t*, tmq_buffer_t*, uint16_t) = {
        parse_add_user_message
};

static void decode_tcp_message_(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    tmq_console_codec_t* console_codec = (tmq_console_codec_t *)codec;
    console_conn_ctx_t* ctx = conn->context;
    assert(ctx != NULL);

    console_parsing_ctx_t* parsing_ctx = &ctx->parsing_ctx;
    do
    {
        switch (parsing_ctx->state)
        {
            case PARSING_HEADER:
                if(buffer->readable_bytes < CONSOLE_HEADER_SIZE)
                    return;
                parse_header(buffer, parsing_ctx);
                parsing_ctx->state = PARSING_PAYLOAD;
            case PARSING_PAYLOAD:
                if(buffer->readable_bytes < parsing_ctx->payload_len)
                    return;
                payload_parsers[parsing_ctx->message_type](console_codec, conn, buffer, parsing_ctx->payload_len);
                parsing_ctx->state = PARSING_HEADER;
        }
    } while(buffer->readable_bytes > 0 && parsing_ctx->state == PARSING_HEADER);
}

extern void add_user(tmq_broker_t* broker, tmq_tcp_conn_t* conn, const char* username, const char* password);

void tmq_console_codec_init(tmq_console_codec_t* codec)
{
    codec->type = SERVER_CODEC;
    codec->decode_tcp_message = decode_tcp_message_;
    codec->on_add_user = add_user;
}

extern void pack_uint16(packet_buf* buf, uint16_t value);

void send_user_operation_reply(tmq_tcp_conn_t* conn, user_op_context_t* ctx)
{
    packet_buf buf = tmq_vec_make(uint8_t);
    uint8_t message_type = ADD_USER;
    uint16_t user_len = tmq_str_len(ctx->username);
    uint16_t reason_len = tmq_str_len(ctx->reason);
    uint16_t payload_len =  user_len + reason_len + 1;
    if(reason_len) payload_len += 1;
    if(!ctx->success)
        message_type |= 0x80;
    tmq_vec_push_back(buf, message_type);
    pack_uint16(&buf, payload_len);
    tmq_vec_reserve(buf, CONSOLE_HEADER_SIZE + payload_len);
    if(ctx->reason)
    {
        memcpy(tmq_vec_end(buf), ctx->reason, reason_len + 1);
        tmq_vec_resize(buf, tmq_vec_size(buf) + reason_len + 1);
    }
    memcpy(tmq_vec_end(buf), ctx->username, user_len + 1);
    tmq_vec_resize(buf, tmq_vec_size(buf) + user_len + 1);
    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}

/************************ called by tinymqtt_shell, using blocking IO **************************/

ssize_t writen(int fd, const char* data, size_t n)
{
    size_t left = n;
    const char* ptr = data;
    ssize_t writen;
    while(left > 0)
    {
        if((writen = tmq_socket_write(fd, ptr, left)) <= 0)
        {
            if(writen < 0 && errno == EINTR)
                writen = 0;
            else
                return -1;
        }
        left -= writen;
        ptr += writen;
    }
    return (ssize_t)n;
}

ssize_t readn(int fd, char* data, size_t n)
{
    size_t left = n;
    char* ptr = data;
    ssize_t nread;
    while(left > 0)
    {
        if((nread = tmq_socket_read(fd, ptr, left)) < 0)
        {
            if(errno == EINTR)
                nread = 0;
            else
                return -1;
        }
        else if(nread == 0)
            break;
        left -= nread;
        ptr += nread;
    }
    return (ssize_t)(n - left);
}

int send_add_user_message(int fd, const char* username, const char* password)
{
    int ret = 0;
    packet_buf buf = tmq_vec_make(uint8_t);
    tmq_vec_push_back(buf, ADD_USER);
    uint16_t user_len = strlen(username);
    uint16_t pwd_len = strlen(password);
    uint16_t payload_len =  user_len + pwd_len + 2;
    pack_uint16(&buf, payload_len);
    tmq_vec_reserve(buf, CONSOLE_HEADER_SIZE + payload_len);
    memcpy(tmq_vec_end(buf), username, user_len + 1);
    tmq_vec_resize(buf, tmq_vec_size(buf) + user_len + 1);
    memcpy(tmq_vec_end(buf), password, pwd_len + 1);
    tmq_vec_resize(buf, tmq_vec_size(buf) + pwd_len + 1);
    if(writen(fd, (char*)tmq_vec_begin(buf), tmq_vec_size(buf)) < 0)
        ret = -1;
    tmq_vec_free(buf);
    return ret;
}

int receive_user_operation_reply(int fd)
{
    uint8_t header[3];
    if(readn(fd, (char*)header, CONSOLE_HEADER_SIZE) < CONSOLE_HEADER_SIZE)
        return -1;
    console_message_type type = header[0];
    uint16_t payload_len = be16toh(*((uint16_t*)(header + 1)));
    char payload[100] = {0};
    char* ptr = payload;
    if(readn(fd, payload, payload_len) < payload_len)
        return -1;
    uint8_t failed = (type & 0x80) != 0;
    char* error = NULL;
    char* username;
    if(failed)
    {
        error = ptr;
        ptr += strlen(error) + 1;
    }
    username = ptr;
    if(!failed)
    {
        if((type & ~0x80) == ADD_USER)
            printf("add user [%s] success\n", username);
    }
    else
    {
        if((type & ~0x80) == ADD_USER)
            printf("add user [%s] failed, error: %s\n", username, error);
    }
    return 0;
}
