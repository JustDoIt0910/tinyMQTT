//
// Created by zr on 23-4-20.
//
#include "mqtt_codec.h"
#include "mqtt_broker.h"
#include "net/mqtt_tcp_conn.h"
#include "base/mqtt_util.h"
#include <stdio.h>

static void decode_tcp_message_(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    tcp_conn_ctx* ctx = conn->context;
    if(ctx->session_state != NO_SESSION)
        ctx->last_msg_time = time_now();


}

void tmq_codec_init(tmq_codec_t* codec)
{
    codec->decode_tcp_message = decode_tcp_message_;
}