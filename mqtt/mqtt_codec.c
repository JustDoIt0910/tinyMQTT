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

static decode_status parse_connect_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                          tmq_buffer_t* buffer, uint32_t len)
{
    if(codec->type != SERVER_CODEC)
        return UNEXPECTED_PACKET;
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
    /* If will-flag is 0, will qos must be set 0. And will qos can't greater than 2 */
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
    ctx->conn_state = STARTING_SESSION;
    codec->on_connect(ctx->upstream.broker, conn, &connect_pkt);
    return DECODE_OK;
}

static decode_status parse_connack_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                          tmq_buffer_t* buffer, uint32_t len)
{
    return DECODE_OK;
}

static decode_status parse_publish_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                          tmq_buffer_t* buffer, uint32_t len)
{
    tmq_publish_pkt publish_pkt;
    tcp_conn_ctx* ctx = conn->context;
    publish_pkt.flags = FLAGS(ctx->parsing_ctx.fixed_header);
    uint16_t topic_name_len;
    tmq_buffer_read16(buffer, &topic_name_len);
    publish_pkt.topic = tmq_str_new_len(NULL, topic_name_len);
    tmq_buffer_read(buffer, publish_pkt.topic, topic_name_len);
    ssize_t payload_len = len - 2 - topic_name_len;

    if(PUBLISH_QOS(publish_pkt.flags) != 0)
    {
        tmq_buffer_read16(buffer, &publish_pkt.packet_id);
        payload_len -= 2;
    }

    if(payload_len < 0)
        return BAD_PACKET_FORMAT;
    publish_pkt.payload = tmq_str_new_len(NULL, payload_len);
    tmq_buffer_read(buffer, publish_pkt.payload, payload_len);

    /* qos = 1, respond with a puback message */
    if(PUBLISH_QOS(publish_pkt.flags) == 1)
    {
        tmq_puback_pkt ack = {
                .packet_id = publish_pkt.packet_id
        };
        send_puback_packet(conn, &ack);
    }
    /* qos = 2, respond with a pubrec message */
    else if(PUBLISH_QOS(publish_pkt.flags) == 2)
    {
        tmq_pubrec_pkt rec = {
                .packet_id = publish_pkt.packet_id
        };
        send_puback_packet(conn, &rec);
    }

    codec->on_publish(ctx->upstream.session, &publish_pkt);
    return DECODE_OK;
}

static decode_status parse_puback_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                         tmq_buffer_t* buffer, uint32_t len)
{
    tmq_puback_pkt puback_pkt;
    tmq_buffer_read16(buffer, &puback_pkt.packet_id);
    tcp_conn_ctx* ctx = conn->context;
    codec->on_pub_ack(ctx->upstream.session, &puback_pkt);
    return DECODE_OK;
}

static decode_status parse_pubrec_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                         tmq_buffer_t* buffer, uint32_t len)
{
    tmq_pubrec_pkt pubrec_pkt;
    tmq_buffer_read16(buffer, &pubrec_pkt.packet_id);
    tcp_conn_ctx* ctx = conn->context;
    codec->on_pub_rec(ctx->upstream.session, &pubrec_pkt);
    return DECODE_OK;
}

static decode_status parse_pubrel_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                         tmq_buffer_t* buffer, uint32_t len)
{
    tmq_pubrel_pkt pubrel_pkt;
    tmq_buffer_read16(buffer, &pubrel_pkt.packet_id);
    tcp_conn_ctx* ctx = conn->context;

    /* respond pub_rel with a pub_comp message */
    tmq_pubcomp_pkt comp = {
            .packet_id = pubrel_pkt.packet_id
    };
    send_pubcomp_packet(conn, &comp);

    codec->on_pub_rel(ctx->upstream.session, &pubrel_pkt);
    return DECODE_OK;
}

static decode_status parse_pubcomp_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                          tmq_buffer_t* buffer, uint32_t len)
{
    tmq_pubcomp_pkt pubcomp_pkt;
    tmq_buffer_read16(buffer, &pubcomp_pkt.packet_id);
    tcp_conn_ctx* ctx = conn->context;
    codec->on_pub_comp(ctx->upstream.session, &pubcomp_pkt);
    return DECODE_OK;
}

static decode_status parse_subscribe_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                            tmq_buffer_t* buffer, uint32_t len)
{
    if(codec->type != SERVER_CODEC)
        return UNEXPECTED_PACKET;
    tmq_subscribe_pkt subscribe_pkt;
    tmq_vec_init(&subscribe_pkt.topics, topic_filter_qos);

    tmq_buffer_read16(buffer, &subscribe_pkt.packet_id);
    len -= 2;
    while(len > 0)
    {
        uint16_t tf_len;
        tmq_buffer_read16(buffer, &tf_len);
        tmq_str_t topic_filter = tmq_str_new_len(NULL, tf_len);
        tmq_buffer_read(buffer, topic_filter, tf_len);
        uint8_t qos = 0;
        tmq_buffer_read(buffer, (char*) &qos, 1);
        len -= 2 + tf_len + 1;
        if(qos > 2)
        {
            tmq_subscribe_pkt_cleanup(&subscribe_pkt);
            return PROTOCOL_ERROR;
        }
        topic_filter_qos pair = {
                .topic_filter = topic_filter,
                .qos = qos
        };
        tmq_vec_push_back(subscribe_pkt.topics, pair);
    }
    tcp_conn_ctx* ctx = conn->context;

    codec->on_subsribe(ctx->upstream.session, &subscribe_pkt);
    return DECODE_OK;
}

static decode_status parse_suback_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                         tmq_buffer_t* buffer, uint32_t len)
{
    if(codec->type != CLIENT_CODEC)
        return UNEXPECTED_PACKET;
    return DECODE_OK;
}

static decode_status parse_unsubscribe_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                              tmq_buffer_t* buffer, uint32_t len)
{
    if(codec->type != SERVER_CODEC)
        return UNEXPECTED_PACKET;
    tmq_unsubscribe_pkt unsubscribe_pkt;
    tmq_vec_init(&unsubscribe_pkt.topics, tmq_str_t);

    tmq_buffer_read16(buffer, &unsubscribe_pkt.packet_id);
    len -= 2;
    while(len > 0)
    {
        uint16_t tf_len;
        tmq_buffer_read16(buffer, &tf_len);
        tmq_str_t topic_filter = tmq_str_new_len(NULL, tf_len);
        tmq_buffer_read(buffer, topic_filter, tf_len);
        len -= 2 + tf_len;
        tmq_vec_push_back(unsubscribe_pkt.topics, topic_filter);
    }
    tcp_conn_ctx* ctx = conn->context;
    codec->on_unsubcribe(ctx->upstream.session, &unsubscribe_pkt);
    return DECODE_OK;
}

static decode_status parse_unsuback_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                           tmq_buffer_t* buffer, uint32_t len)
{
    if(codec->type != SERVER_CODEC)
        return UNEXPECTED_PACKET;
    return DECODE_OK;
}

static decode_status parse_pingreq_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                          tmq_buffer_t* buffer, uint32_t len)
{
    if(codec->type != SERVER_CODEC)
        return UNEXPECTED_PACKET;
    send_pingresp_packet(conn, NULL);
    tcp_conn_ctx* ctx = conn->context;
    codec->on_ping_req(ctx->upstream.session);
    return DECODE_OK;
}

static decode_status parse_pingresp_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                           tmq_buffer_t* buffer, uint32_t len)
{
    return DECODE_OK;
}

static decode_status parse_disconnect_packet(tmq_codec_t* codec, tmq_tcp_conn_t* conn,
                                             tmq_buffer_t* buffer, uint32_t len)
{
    tcp_conn_ctx* ctx = conn->context;
    int in_session = ctx->conn_state == IN_SESSION;
    ctx->conn_state = NO_SESSION;
    tmq_session_t* session = ctx->upstream.session;
    ctx->upstream.broker = conn->group->broker;
    if(in_session)
        codec->on_disconnect(ctx->upstream.broker, session);
    return DECODE_OK;
}

static decode_status(*packet_parsers[])(tmq_codec_t*, tmq_tcp_conn_t*, tmq_buffer_t*, uint32_t) = {
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
    if(ctx->conn_state != NO_SESSION)
        ctx->last_msg_time = time_now();

    pkt_parsing_ctx* parsing_ctx = &ctx->parsing_ctx;
    decode_status status;
    do
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
                /* guarantee that a CONNECT packet is the first packet received,
                 * and a DISCONNECT is the last packet received. */
                if(ctx->conn_state == NO_SESSION && PACKET_TYPE(parsing_ctx->fixed_header) != MQTT_CONNECT)
                {
                    status = PROTOCOL_ERROR;
                    break;
                }

            case PARSING_REMAIN_LENGTH:
                if(buffer->readable_bytes == 0)
                    break;
                status = parse_remain_length(buffer, parsing_ctx);
                if(status == DECODE_OK)
                    parsing_ctx->state = PARSING_BODY;
                else break;

            case PARSING_BODY:
                if(buffer->readable_bytes < parsing_ctx->fixed_header.remain_length)
                {
                    status = NEED_MORE_DATA;
                    break;
                }
                status = packet_parsers[PACKET_TYPE(parsing_ctx->fixed_header)](codec, conn,
                        buffer, parsing_ctx->fixed_header.remain_length);
                if(status == DECODE_OK)
                    parsing_ctx->state = PARSING_FIXED_HEADER;
                else break;
        }
        if(status != DECODE_OK && status != NEED_MORE_DATA)
        {
            tmq_tcp_conn_close(get_ref(conn));
            break;
        }
    } while(buffer->readable_bytes > 0 && parsing_ctx->state == PARSING_FIXED_HEADER);
}

extern void mqtt_connect_request(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_connect_pkt* connect_pkt);
extern void mqtt_disconnect_request(tmq_broker_t* broker, tmq_session_t* session);

extern void tmq_session_handle_subscribe(tmq_session_t* session, tmq_subscribe_pkt* subscribe_pkt);
extern void tmq_session_handle_unsubscribe(tmq_session_t* session, tmq_unsubscribe_pkt* unsubscribe_pkt);
extern void tmq_session_handle_publish(tmq_session_t* session, tmq_publish_pkt* publish_pkt);
extern void tmq_session_handle_puback(tmq_session_t* session, tmq_puback_pkt* puback_pkt);
extern void tmq_session_handle_pubrec(tmq_session_t* session, tmq_pubrec_pkt* pubrec_pkt);
extern void tmq_session_handle_pubrel(tmq_session_t* session, tmq_pubrel_pkt* pubrel_pkt);
extern void tmq_session_handle_pubcomp(tmq_session_t* session, tmq_pubcomp_pkt* pubcomp_pkt);
extern void tmq_session_handle_pingreq(tmq_session_t* session);

void tmq_codec_init(tmq_codec_t* codec, tmq_codec_type type)
{
    codec->type = type;
    codec->decode_tcp_message = decode_tcp_message_;
    codec->on_connect = mqtt_connect_request;
    codec->on_disconnect = mqtt_disconnect_request;
    codec->on_subsribe = tmq_session_handle_subscribe;
    codec->on_unsubcribe = tmq_session_handle_unsubscribe;
    codec->on_publish = tmq_session_handle_publish;
    codec->on_pub_ack = tmq_session_handle_puback;
    codec->on_pub_rec = tmq_session_handle_pubrec;
    codec->on_pub_rel = tmq_session_handle_pubrel;
    codec->on_pub_comp = tmq_session_handle_pubcomp;
    codec->on_ping_req = tmq_session_handle_pingreq;
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

void send_connect_packet(tmq_tcp_conn_t* conn, void* pkt)
{

}

void send_connack_packet(tmq_tcp_conn_t* conn, void* pkt)
{
    tmq_connack_pkt* connack_pkt = pkt;
    packet_buf buf = tmq_vec_make(uint8_t);
    if(make_fixed_header(MQTT_CONNACK, 0, 2, &buf) < 0)
    {
        tmq_vec_free(buf);
        return;
    }
    tmq_vec_push_back(buf, connack_pkt->ack_flags);
    tmq_vec_push_back(buf, connack_pkt->return_code);
    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}

static void pack_uint16(packet_buf* buf, uint16_t value)
{
    uint16_t value_be = htobe16(value);
    tmq_vec_push_back(*buf, value_be & 0xFF);  /* MSB */
    tmq_vec_push_back(*buf, (value_be >> 8) & 0xFF);  /* LSB */
}

void send_publish_packet(tmq_tcp_conn_t* conn, void* pkt)
{
    tmq_publish_pkt* publish_pkt = pkt;
    packet_buf buf = tmq_vec_make(uint8_t);
    size_t topic_len = tmq_str_len(publish_pkt->topic);
    size_t payload_len = tmq_str_len(publish_pkt->payload);
    uint32_t remain_len = 4 + topic_len + payload_len;
    /* qos o messages have no packet_id */
    if(PUBLISH_QOS(publish_pkt->flags) == 0)
        remain_len -= 2;
    if(make_fixed_header(MQTT_PUBLISH, publish_pkt->flags, remain_len, &buf) < 0)
    {
        tmq_vec_free(buf);
        return;
    }
    tmq_vec_reserve(buf, tmq_vec_size(buf) + remain_len);
    pack_uint16(&buf, topic_len);

    memcpy(tmq_vec_end(buf), publish_pkt->topic, topic_len);
    tmq_vec_resize(buf, tmq_vec_size(buf) + topic_len);

    if(PUBLISH_QOS(publish_pkt->flags) > 0)
        pack_uint16(&buf, publish_pkt->packet_id);
    memcpy(tmq_vec_end(buf), publish_pkt->payload, payload_len);
    tmq_vec_resize(buf, tmq_vec_size(buf) + payload_len);

    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}

void send_puback_packet(tmq_tcp_conn_t* conn, void* pkt)
{
    tmq_puback_pkt* puback_pkt = pkt;
    packet_buf buf = tmq_vec_make(uint8_t);
    if(make_fixed_header(MQTT_PUBACK, 0, 2, &buf) < 0)
    {
        tmq_vec_free(buf);
        return;
    }
    pack_uint16(&buf, puback_pkt->packet_id);

    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}

void send_pubrec_packet(tmq_tcp_conn_t* conn, void* pkt)
{
    tmq_pubrec_pkt * pubrec_pkt = pkt;
    packet_buf buf = tmq_vec_make(uint8_t);
    if(make_fixed_header(MQTT_PUBREC, 0, 2, &buf) < 0)
    {
        tmq_vec_free(buf);
        return;
    }
    pack_uint16(&buf, pubrec_pkt->packet_id);

    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}

void send_pubrel_packet(tmq_tcp_conn_t* conn, void* pkt)
{
    tmq_pubrec_pkt * pubrel_pkt = pkt;
    packet_buf buf = tmq_vec_make(uint8_t);
    if(make_fixed_header(MQTT_PUBREL, 2, 2, &buf) < 0)
    {
        tmq_vec_free(buf);
        return;
    }
    pack_uint16(&buf, pubrel_pkt->packet_id);

    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}

void send_pubcomp_packet(tmq_tcp_conn_t* conn, void* pkt)
{
    tmq_pubcomp_pkt * pubcomp_pkt = pkt;
    packet_buf buf = tmq_vec_make(uint8_t);
    if(make_fixed_header(MQTT_PUBCOMP, 0, 2, &buf) < 0)
    {
        tmq_vec_free(buf);
        return;
    }
    pack_uint16(&buf, pubcomp_pkt->packet_id);

    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}

void send_subscribe_packet(tmq_tcp_conn_t* conn, void* pkt)
{

}

void send_suback_packet(tmq_tcp_conn_t* conn, void* pkt)
{
    tmq_suback_pkt* suback_pkt = pkt;
    packet_buf buf = tmq_vec_make(uint8_t);
    uint32_t payload_len = tmq_vec_size(suback_pkt->return_codes);
    if(make_fixed_header(MQTT_SUBACK, 0, 2 + payload_len, &buf) < 0)
    {
        tmq_vec_free(buf);
        return;
    }
    pack_uint16(&buf, suback_pkt->packet_id);

    uint8_t* code = tmq_vec_begin(suback_pkt->return_codes);
    for(; code != tmq_vec_end(suback_pkt->return_codes); code++)
        tmq_vec_push_back(buf, *code);
    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}

void send_unsubscribe_packet(tmq_tcp_conn_t* conn, void* pkt)
{

}

void send_unsuback_packet(tmq_tcp_conn_t* conn, void* pkt)
{
    tmq_unsuback_pkt* unsuback_pkt = pkt;
    packet_buf buf = tmq_vec_make(uint8_t);
    if(make_fixed_header(MQTT_UNSUBACK, 0, 2, &buf) < 0)
    {
        tmq_vec_free(buf);
        return;
    }
    pack_uint16(&buf, unsuback_pkt->packet_id);

    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}

void send_pingreq_packet(tmq_tcp_conn_t* conn, void* pkt)
{

}

void send_pingresp_packet(tmq_tcp_conn_t* conn, void* pkt)
{
    packet_buf buf = tmq_vec_make(uint8_t);
    if(make_fixed_header(MQTT_PINGRESP, 0, 0, &buf) < 0)
    {
        tmq_vec_free(buf);
        return;
    }
    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}

void send_disconnect_packet(tmq_tcp_conn_t* conn, void* pkt)
{

}

static void(*packet_senders[])(tmq_tcp_conn_t*, void*) = {
        NULL, send_connect_packet, send_connack_packet,
        send_publish_packet, send_puback_packet, send_pubrec_packet, send_pubrel_packet, send_pubcomp_packet,
        send_subscribe_packet, send_suback_packet, send_unsubscribe_packet, send_unsuback_packet,
        send_pingreq_packet, send_pingresp_packet,
        send_disconnect_packet
};

void send_any_packet(tmq_tcp_conn_t* conn, tmq_any_packet_t* pkt)
{
    packet_senders[pkt->packet_type](conn, pkt->packet);
}