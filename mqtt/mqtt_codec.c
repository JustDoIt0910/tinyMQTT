//
// Created by zr on 23-4-20.
//
#include "mqtt_codec.h"
#include "mqtt_broker.h"
#include "net/mqtt_tcp_conn.h"
#include <assert.h>
#include <endian.h>
#include <string.h>

static decode_status parse_fix_header(tmq_buffer_t* buffer, pkt_parsing_ctx* parsing_ctx)
{
    uint8_t byte;
    tmq_buffer_read(buffer, (char*) &byte, 1);
    /* invalid packet type, close the connection */
    if(byte < 1 || byte > 14)
        return UNKNOWN_PACKET;
    parsing_ctx->fixed_header.type_flags = byte;
    parsing_ctx->fixed_header.remain_length = 0;
    parsing_ctx->multiplier = 1;
    parsing_ctx->state = PARSING_REMAIN_LENGTH;
    return DECODE_OK;
}

static decode_status validate_flags(tmq_fixed_header* header)
{
    uint8_t type = PACKET_TYPE(*header);
    if(type == MQTT_PUBLISH)
        return DECODE_OK;
    if(type == MQTT_PUBREL || type == MQTT_SUBSCRIBE || type == MQTT_UNSUBSCRIBE)
        return FLAGS(*header) == 2 ? DECODE_OK : BAD_PACKET_FORMAT;
    return FLAGS(*header) == 0 ? DECODE_OK : PROTOCOL_ERROR;
}

static decode_status parse_remain_length(tmq_buffer_t* buffer, pkt_parsing_ctx* parsing_ctx)
{
    uint8_t byte;
    do
    {
        tmq_buffer_read(buffer, (char*) &byte, 1);
        parsing_ctx->fixed_header.remain_length += (byte & 0x7F) * parsing_ctx->multiplier;
        if(parsing_ctx->multiplier > 128 * 128 * 128)
            return BAD_PACKET_FORMAT;
        parsing_ctx->multiplier *= 128;
    } while (buffer->readable_bytes > 0 && (byte & 0x80));
    if(byte & 0x80)
        return NEED_MORE_DATA;
    return DECODE_OK;
}

static decode_status parse_connect_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    /* parse variable header */
    uint16_t protocol_nam_len;
    tmq_buffer_read(buffer, (char*) &protocol_nam_len, 2);
    protocol_nam_len = be16toh(protocol_nam_len);
    if(protocol_nam_len != 4)
        return PROTOCOL_ERROR;
    char protocol_name[5] = {0};
    tmq_buffer_read(buffer, protocol_name, 4);
    if(!strcmp(protocol_name, "MQTT"))
        return PROTOCOL_ERROR;
    uint8_t protocol_level;
    tmq_buffer_read(buffer, (char*) &protocol_level, 1);
    if(protocol_level != 4)
        return UNSUPPORTED_VERSION;
    uint8_t flags;
    tmq_buffer_read(buffer, (char*) &flags, 1);
    if(CONNECT_RESERVED(flags))
        return PROTOCOL_ERROR;
    uint16_t keep_alive;
    tmq_buffer_read(buffer, (char*) &keep_alive, 2);
    keep_alive = be16toh(keep_alive);

    /* parse payload */
    return DECODE_OK;
}

static decode_status parse_connack_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_publish_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_puback_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_pubrec_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_pubrel_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_pubcomp_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_subscribe_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_suback_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_unsubscribe_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_unsuback_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_pingreq_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_pingresp_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_disconnect_packet(tmq_codec_t* codec, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status(*packet_parsers[])(tmq_codec_t*, tmq_buffer_t*) = {
        NULL, parse_connect_packet, parse_connack_packet,
        parse_publish_packet, parse_puback_packet, parse_pubrec_packet, parse_pubrel_packet, parse_pubcomp_packet,
        parse_subscribe_packet, parse_suback_packet, parse_unsubscribe_packet, parse_unsuback_packet,
        parse_pingreq_packet, parse_pingresp_packet,
        parse_disconnect_packet
};

static void decode_tcp_message_(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    tcp_conn_ctx* ctx = conn->context;
    assert(ctx != NULL);
    if(ctx->session_state != NO_SESSION)
        ctx->last_msg_time = time_now();

    pkt_parsing_ctx* parsing_ctx = &ctx->parsing_ctx;
    decode_status status;
    while(buffer->readable_bytes > 0 && parsing_ctx->state == PARSING_FIXED_HEADER)
    {
        switch (parsing_ctx->state)
        {
            case PARSING_FIXED_HEADER:
                status = parse_fix_header(buffer, parsing_ctx);
                if(status != DECODE_OK)
                    tmq_tcp_conn_close(conn);
                status = validate_flags(&parsing_ctx->fixed_header);
                if(status == BAD_PACKET_FORMAT)
                    tmq_tcp_conn_close(conn);

            case PARSING_REMAIN_LENGTH:
                if(buffer->readable_bytes == 0)
                    break;
                status = parse_remain_length(buffer, parsing_ctx);
                if(status == DECODE_OK)
                    parsing_ctx->state = PARSING_BODY;
                else if(status == BAD_PACKET_FORMAT)
                    tmq_tcp_conn_close(conn);

            case PARSING_BODY:
                if(buffer->readable_bytes < parsing_ctx->fixed_header.remain_length)
                    break;
                status = packet_parsers[PACKET_TYPE(parsing_ctx->fixed_header)](codec, buffer);
                if(status == DECODE_OK)
                    parsing_ctx->state = PARSING_FIXED_HEADER;
                else
                {
                    /* todo: handle parsing error */
                }
        }
    }
}

void tmq_codec_init(tmq_codec_t* codec)
{
    codec->decode_tcp_message = decode_tcp_message_;
}

void send_connect_packet(tmq_tcp_conn_t* conn, tmq_connect_pkt* pkt)
{

}

void send_connack_packet(tmq_tcp_conn_t* conn, tmq_connack_pkt* pkt)
{
    
}

void send_publish_packet(tmq_tcp_conn_t* conn, tmq_publish_pkt* pkt)
{

}

void send_puback_packet(tmq_tcp_conn_t* conn, tmq_puback_pkt* pkt)
{

}

void send_pubrec_packet(tmq_tcp_conn_t* conn, tmq_pubrec_pkt* pkt)
{

}

void send_pubrel_packet(tmq_tcp_conn_t* conn, tmq_pubrel_pkt* pkt)
{

}

void send_pubcomp_packet(tmq_tcp_conn_t* conn, tmq_pubcomp_pkt* pkt)
{

}

void send_subscribe_packet(tmq_tcp_conn_t* conn, tmq_subscribe_pkt* pkt)
{

}

void send_suback_packet(tmq_tcp_conn_t* conn, tmq_suback_pkt* pkt)
{

}

void send_unsubscribe_packet(tmq_tcp_conn_t* conn, tmq_unsubscribe_pkt* pkt)
{

}

void send_unsuback_packet(tmq_tcp_conn_t* conn, tmq_unsuback_pkt* pkt)
{

}

void send_pingreq_packet(tmq_tcp_conn_t* conn, tmq_pingreq_pkt* pkt)
{

}

void send_pingresp_packet(tmq_tcp_conn_t* conn, tmq_pingresp_pkt* pkt)
{

}

void send_disconnect_packet(tmq_tcp_conn_t* conn, tmq_disconnect_pkt* pkt)
{

}