//
// Created by zr on 23-4-20.
//
#include "mqtt_codec.h"
#include "mqtt_broker.h"
#include "net/mqtt_tcp_conn.h"
#include "tlog.h"
#include <assert.h>
#include <string.h>

static decode_status parse_fix_header(tmq_buffer_t* buffer, pkt_parsing_ctx* parsing_ctx)
{
    uint8_t byte;
    tmq_buffer_read(buffer, (char*) &byte, 1);
    /* invalid packet type, close the connection */
    uint type = (byte >> 4) & 0x0F;
    if(type < 1 || type > 14)
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

static decode_status parse_connect_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    /* parse variable header */
    uint16_t protocol_nam_len;
    tmq_buffer_read16(buffer, &protocol_nam_len);
    if(protocol_nam_len != 4)
        return PROTOCOL_ERROR;
    char protocol_name[5] = {0};
    /* read and check if the protocol name is correct */
    tmq_buffer_read(buffer, protocol_name, 4);
    if(strcmp(protocol_name, "MQTT") != 0)
        return PROTOCOL_ERROR;

    uint8_t protocol_level;
    tmq_buffer_read(buffer, (char*) &protocol_level, 1);
    /* check if the protocol level is 4.
     * If not, a CONNACK packet with return code 0x01 will be sent */
    if(protocol_level != 4)
    {
        tmq_connack_pkt pkt;
        pkt.ack_flags = 0;
        pkt.return_code = UNACCEPTABLE_PROTOCOL_VERSION;
        send_connack_packet(conn, &pkt);
        return PROTOCOL_ERROR;
    }

    uint8_t flags;
    tmq_buffer_read(buffer, (char*) &flags, 1);
    if(CONNECT_RESERVED(flags))
        return PROTOCOL_ERROR;
    /* If will flag is 0, will qos must be set 0. And will qos can't greater than 2 */
    if((!CONNECT_WILL_FLAG(flags) && CONNECT_WILL_QOS(flags)) || CONNECT_WILL_QOS(flags) > 2)
        return PROTOCOL_ERROR;

    uint16_t keep_alive;
    tmq_buffer_read16(buffer, &keep_alive);

    /* parse payload */
    uint16_t client_id_len;
    tmq_buffer_read16(buffer, &client_id_len);
    /* if client provide a zero-byte ClientId, it must set clean_session to 1,
     * otherwise the server returns a CONNACK packet will return code 0x02 */
    if(!client_id_len && !CONNECT_CLEAN_SESSION(flags))
    {
        tmq_connack_pkt pkt;
        pkt.ack_flags = 0;
        pkt.return_code = IDENTIFIER_REJECTED;
        send_connack_packet(conn, &pkt);
        return PROTOCOL_ERROR;
    }
    tmq_connect_pkt connect_pkt;
    bzero(&connect_pkt, sizeof(tmq_connect_pkt));
    connect_pkt.flags = flags;
    connect_pkt.keep_alive = keep_alive;
    if(client_id_len > 0)
    {
        /* read client identifier */
        connect_pkt.client_id = tmq_str_new_len(NULL, client_id_len);
        tmq_buffer_read(buffer, connect_pkt.client_id, client_id_len);
    }
    if(CONNECT_WILL_FLAG(flags))
    {
        /* read will topic and will message */
        uint16_t field_len;
        tmq_buffer_read16(buffer, &field_len);
        connect_pkt.will_topic = tmq_str_new_len(NULL, field_len);
        tmq_buffer_read(buffer, connect_pkt.will_topic, field_len);
        tmq_buffer_read16(buffer, &field_len);
        connect_pkt.will_message = tmq_str_new_len(NULL, field_len);
        tmq_buffer_read(buffer, connect_pkt.will_message, field_len);
    }
    if(CONNECT_USERNAME_FLAG(flags))
    {
        /* read username if username flag is 1 */
        uint16_t username_len;
        tmq_buffer_read16(buffer, &username_len);
        connect_pkt.username = tmq_str_new_len(NULL, username_len);
        tmq_buffer_read(buffer, connect_pkt.username, username_len);
    }
    if(CONNECT_PASSWORD_FLAG(flags))
    {
        /* read password if password flag is 1 */
        uint16_t password_len;
        tmq_buffer_read16(buffer, &password_len);
        connect_pkt.password = tmq_str_new_len(NULL, password_len);
        tmq_buffer_read(buffer, connect_pkt.password, password_len);
    }
    tcp_conn_ctx* ctx = conn->context;
    /* deliver this CONNECT packet to the broker */
    codec->on_connect(ctx->upstream.broker, conn, connect_pkt);
    return DECODE_OK;
}

static decode_status parse_connack_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_publish_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_puback_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_pubrec_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_pubrel_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_pubcomp_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_subscribe_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_suback_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_unsubscribe_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_unsuback_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_pingreq_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_pingresp_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status parse_disconnect_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    return DECODE_OK;
}

static decode_status(*packet_parsers[])(tmq_codec_t*, tmq_tcp_conn_t*, tmq_buffer_t*) = {
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
                    break;
                status = validate_flags(&parsing_ctx->fixed_header);
                if(status == BAD_PACKET_FORMAT)
                    break;

            case PARSING_REMAIN_LENGTH:
                if(buffer->readable_bytes == 0)
                    break;
                status = parse_remain_length(buffer, parsing_ctx);
                if(status == DECODE_OK)
                    parsing_ctx->state = PARSING_BODY;
                else if(status == BAD_PACKET_FORMAT)
                    break;

            case PARSING_BODY:
                if(buffer->readable_bytes < parsing_ctx->fixed_header.remain_length)
                    break;
                status = packet_parsers[PACKET_TYPE(parsing_ctx->fixed_header)](codec, conn, buffer);
                if(status == DECODE_OK)
                    parsing_ctx->state = PARSING_FIXED_HEADER;
                else break;
        }
        if(status != DECODE_OK && status != NEED_MORE_DATA)
        {
            tmq_tcp_conn_close(get_ref(conn));
            break;
        }
    }
}

extern void mqtt_connect_request(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_connect_pkt connect_pkt);

void tmq_codec_init(tmq_codec_t* codec)
{
    codec->decode_tcp_message = decode_tcp_message_;
    codec->on_connect = mqtt_connect_request;
}

typedef tmq_vec(uint8_t) packet_buf;
int make_fixed_header(tmq_packet_type type, uint8_t flags, uint32_t remain_length, packet_buf* buf)
{
    uint8_t type_flags = type;
    type_flags = (type_flags << 4) | (flags & 0x0F);
    tmq_vec_push_back(*buf, type_flags);
    uint8_t byte;
    do
    {
        byte = remain_length % 128;
        remain_length /= 128;
        if(remain_length > 0)
            byte = byte | 0x80;
        tmq_vec_push_back(*buf, byte);
    } while(remain_length > 0);
    if(tmq_vec_size(*buf) > 5)
    {
        tlog_error("make_fixed_header() error: remain length is too large");
        return -1;
    }
    return 0;
}

void send_connect_packet(tmq_tcp_conn_t* conn, tmq_connect_pkt* pkt)
{

}

void send_connack_packet(tmq_tcp_conn_t* conn, tmq_connack_pkt* pkt)
{
    packet_buf buf = tmq_vec_make(uint8_t);
    if(make_fixed_header(MQTT_CONNACK, 0, 2, &buf) < 0)
    {
        tmq_vec_free(buf);
        return;
    }
    tmq_vec_push_back(buf, pkt->ack_flags);
    tmq_vec_push_back(buf, pkt->return_code);
    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
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