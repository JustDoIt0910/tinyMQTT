//
// Created by zr on 23-4-20.
//
#include "mqtt_codec.h"
#include "mqtt_broker.h"
#include "net/mqtt_tcp_conn.h"
#include <assert.h>

static decode_status parse_remain_length(tmq_buffer_t* buffer, pkt_parsing_ctx* parsing_ctx)
{
    uint8_t byte; uint32_t mul = 1;
    do
    {
        tmq_buffer_read(buffer, (char*) &byte, 1);
        parsing_ctx->fixed_header.remain_length += (byte & 0x7F) * mul;
        if(mul > 128 * 128 * 128)
            return BAD_PACKET_FORMAT;
        mul *= 128;
    } while (buffer->readable_bytes > 0 && (byte & 0x80));
    return DECODE_OK;
}

static void decode_tcp_message_(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    tcp_conn_ctx* ctx = conn->context;
    assert(ctx != NULL);
    if(ctx->session_state != NO_SESSION)
        ctx->last_msg_time = time_now();

    pkt_parsing_ctx* parsing_ctx = &ctx->parsing_ctx;
    uint8_t byte; decode_status status;
    while(buffer->readable_bytes > 0)
    {
        switch (parsing_ctx->state)
        {
            case PARSING_FIXED_HEADER:
                tmq_buffer_read(buffer, (char*) &byte, 1);
                /* invalid packet type, close the connection */
                if(byte < 1 || byte > 14)
                    tmq_tcp_conn_close(conn);
                parsing_ctx->fixed_header.type_flags = byte;
                parsing_ctx->fixed_header.remain_length = 0;
                parsing_ctx->state = PARSING_REMAIN_LENGTH;

            case PARSING_REMAIN_LENGTH:
                if(buffer->readable_bytes == 0)
                    break;
                status = parse_remain_length(buffer, parsing_ctx);
        }
    }
}

void tmq_codec_init(tmq_codec_t* codec)
{
    codec->decode_tcp_message = decode_tcp_message_;
}