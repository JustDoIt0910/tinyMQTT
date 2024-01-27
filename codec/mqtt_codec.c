//
// Created by just do it on 2024/1/25.
//
#include "mqtt_codec.h"
#include "mqtt/mqtt_types.h"
#include "net/mqtt_tcp_conn.h"
#include "base/mqtt_util.h"
#include <assert.h>
#include <stdlib.h>

static void parse_header(tmq_buffer_t* buffer, len_based_parsing_ctx_t* parsing_ctx)
{
    tmq_buffer_read(buffer, (char*)&parsing_ctx->message_type, 1);
    tmq_buffer_read16(buffer, &parsing_ctx->payload_len);
}

static void decode_tcp_message_(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    tmq_len_based_codec_t* len_based_codec = (tmq_len_based_codec_t *)codec;
    tcp_conn_simple_ctx_t* ctx = conn->context;
    assert(ctx != NULL);
    len_based_parsing_ctx_t* parsing_ctx = &ctx->parsing_ctx;
    do
    {
        switch (parsing_ctx->state)
        {
            case PARSING_HEADER:
                if(buffer->readable_bytes < LEN_BASED_HEADER_SIZE)
                    return;
                parse_header(buffer, parsing_ctx);
                parsing_ctx->state = PARSING_PAYLOAD;
            case PARSING_PAYLOAD:
                if(buffer->readable_bytes < parsing_ctx->payload_len)
                    return;
                len_based_codec->payload_parsers[parsing_ctx->message_type](len_based_codec, conn,
                        buffer, parsing_ctx->payload_len);
                parsing_ctx->state = PARSING_HEADER;
        }
    } while(buffer->readable_bytes > 0 && parsing_ctx->state == PARSING_HEADER);
}

void tmq_len_based_codec_init(tmq_len_based_codec_t* codec)
{
    codec->decode_tcp_message = decode_tcp_message_;
    codec->payload_parsers = malloc(10 * sizeof(payload_parser));
    codec->n_parsers = 10;
}

void tmq_len_based_codec_register_parser(tmq_len_based_codec_t* codec, len_based_message_type message_type,
                                         payload_parser parser)
{
    if(codec->n_parsers <= message_type)
    {
        payload_parser* p = realloc(codec->payload_parsers, 2 * codec->n_parsers);
        if(!p)
            fatal_error("realloc() error: out of memory");
        codec->payload_parsers = p;
    }
    codec->payload_parsers[message_type] = parser;
}