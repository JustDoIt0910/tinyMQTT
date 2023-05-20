//
// Created by zr on 23-4-20.
//
#include "mqtt_codec.h"
#include "mqtt_broker.h"
#include "net/mqtt_tcp_conn.h"
#include "base/mqtt_util.h"
#include <stdio.h>

static decode_status decode_fix_header(tmq_fixed_header* fixed_header)
{

    return DECODE_OK;
}

static void decode_tcp_message_(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    tcp_conn_ctx* ctx = conn->context;
    if(ctx->session_state != NO_SESSION)
        ctx->last_msg_time = time_now();

    char data[1024] = {0};
    size_t n = tmq_buffer_read(buffer, data, buffer->readable_bytes);
    tlog_info("%s", data);
    tmq_tcp_conn_write(conn, data, n);
}

void tmq_codec_init(tmq_codec_t* codec)
{
    codec->decode_tcp_message = decode_tcp_message_;
}