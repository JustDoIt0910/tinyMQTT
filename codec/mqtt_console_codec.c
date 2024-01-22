//
// Created by just do it on 2024/1/18.
//
#include "mqtt_console_codec.h"
#include "net/mqtt_tcp_conn.h"
#include "mqtt/mqtt_types.h"
#include <assert.h>
#include <string.h>
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
    codec->on_add_user(ctx->broker, user, pwd);
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

extern void add_user(tmq_broker_t* broker, const char* username, const char* password);

void tmq_console_codec_init(tmq_console_codec_t* codec)
{
    codec->type = SERVER_CODEC;
    codec->decode_tcp_message = decode_tcp_message_;
    codec->on_add_user = add_user;
}

ssize_t writen(int fd, const char* data, size_t n)
{
    size_t left = n;
    const char* ptr = data;
    ssize_t writen;
    while(left > 0)
    {
        if((writen = tmq_socket_write(fd, data, n)) <= 0)
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

extern void pack_uint16(packet_buf* buf, uint16_t value);
void send_add_user_message(int fd, const char* username, const char* password)
{
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
    writen(fd, (char*)tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}