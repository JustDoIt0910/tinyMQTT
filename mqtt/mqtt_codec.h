//
// Created by zr on 23-4-20.
//

#ifndef TINYMQTT_MQTT_CODEC_H
#define TINYMQTT_MQTT_CODEC_H
#include "mqtt_packet.h"

typedef struct tmq_codec_s tmq_codec_t;
typedef struct tmq_tcp_conn_s tmq_tcp_conn_t;
typedef struct tmq_buffer_s tmq_buffer_t;
typedef struct tmq_broker_s tmq_broker_t;
typedef struct tmq_session_s tmq_session_t;

typedef struct tmq_fixed_header
{
    uint8_t type_flags;
    uint32_t remain_length;
} tmq_fixed_header;

#define PACKET_TYPE(header) (((header).type_flags >> 4) & 0x0F)
#define FLAGS(header)       (((header).type_flags) & 0x0F)
#define DUP(header)         (((header).type_flags) & 0x08)
#define QOS(header)         (((header).type_flags) & 0x06)
#define RETAIN(header)      (((header).type_flags) & 0x01)

typedef void (*tcp_message_decoder_f) (tmq_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer);
typedef void (*connect_pkt_cb) (tmq_broker_t* broker, tmq_connect_pkt connect_pkt);
typedef void (*connack_pkt_cb) (tmq_session_t* session, tmq_connack_pkt connack_pkt);
typedef void (*publish_pkt_cb) (tmq_session_t* session, tmq_publish_pkt publish_pkt);
typedef void (*puback_pkt_cb) (tmq_session_t* session, tmq_puback_pkt puback_pkt);
typedef void (*pubrec_pkt_cb) (tmq_session_t* session, tmq_pubrec_pkt pubrec_pkt);
typedef void (*pubrel_pkt_cb) (tmq_session_t* session, tmq_pubrel_pkt pubrel_pkt);
typedef void (*pubcomp_pkt_cb) (tmq_session_t* session, tmq_pubcomp_pkt pubcomp_pkt);
typedef void (*subscribe_pkt_cb) (tmq_session_t* session, tmq_subscribe_pkt subscribe_pkt);
typedef void (*suback_pkt_cb) (tmq_session_t* session, tmq_suback_pkt suback_pkt);
typedef void (*unsubscribe_pkt_cb) (tmq_session_t* session, tmq_unsubscribe_pkt unsubscribe_pkt);
typedef void (*unsuback_pkt_cb) (tmq_session_t* session, tmq_unsuback_pkt unsuback_pkt);
typedef void (*pingreq_pkt_cb) (tmq_session_t* session, tmq_pingreq_pkt pingreq_pkt);
typedef void (*pingresp_pkt_cb) (tmq_session_t* session, tmq_pingresp_pkt pingresp_pkt);
typedef void (*disconnect_pkt_cb) (tmq_session_t* session, tmq_disconnect_pkt disconnect_pkt);

typedef struct tmq_codec_s
{
    tcp_message_decoder_f decode_tcp_message;

    connect_pkt_cb on_connect;
    connack_pkt_cb on_conn_ack;

    publish_pkt_cb on_publish;
    puback_pkt_cb on_pub_ack;
    pubrec_pkt_cb on_pub_rec;
    pubrel_pkt_cb on_pub_rel;
    pubcomp_pkt_cb on_pub_comp;

    subscribe_pkt_cb on_subsribe;
    suback_pkt_cb on_sub_ack;
    unsubscribe_pkt_cb on_unsubcribe;
    unsuback_pkt_cb on_unsub_ack;

    pingreq_pkt_cb on_ping_req;
    pingresp_pkt_cb on_ping_resp;

    disconnect_pkt_cb on_disconnect;
} tmq_codec_t;

typedef enum decode_status_e
{
    DECODE_OK,
    NEED_MORE_DATA,
    UNKNOWN_PACKET,
    BAD_PACKET_FORMAT,
    PROTOCOL_ERROR,
    UNSUPPORTED_VERSION
} decode_status;

typedef enum parsing_state_e
{
    PARSING_FIXED_HEADER,
    PARSING_REMAIN_LENGTH,
    PARSING_BODY
} parsing_state;

typedef struct pkt_parsing_ctx_s
{
    parsing_state state;
    tmq_fixed_header fixed_header;
    /* save the current multiplier when decoding remain_length */
    uint32_t multiplier;
} pkt_parsing_ctx;

void tmq_codec_init(tmq_codec_t* codec);

void send_connect_packet(tmq_tcp_conn_t* conn, tmq_connect_pkt* pkt);
void send_connack_packet(tmq_tcp_conn_t* conn, tmq_connack_pkt* pkt);
void send_publish_packet(tmq_tcp_conn_t* conn, tmq_publish_pkt* pkt);
void send_puback_packet(tmq_tcp_conn_t* conn, tmq_puback_pkt* pkt);
void send_pubrec_packet(tmq_tcp_conn_t* conn, tmq_pubrec_pkt* pkt);
void send_pubrel_packet(tmq_tcp_conn_t* conn, tmq_pubrel_pkt* pkt);
void send_pubcomp_packet(tmq_tcp_conn_t* conn, tmq_pubcomp_pkt* pkt);
void send_subscribe_packet(tmq_tcp_conn_t* conn, tmq_subscribe_pkt* pkt);
void send_suback_packet(tmq_tcp_conn_t* conn, tmq_suback_pkt* pkt);
void send_unsubscribe_packet(tmq_tcp_conn_t* conn, tmq_unsubscribe_pkt* pkt);
void send_unsuback_packet(tmq_tcp_conn_t* conn, tmq_unsuback_pkt* pkt);
void send_pingreq_packet(tmq_tcp_conn_t* conn, tmq_pingreq_pkt* pkt);
void send_pingresp_packet(tmq_tcp_conn_t* conn, tmq_pingresp_pkt* pkt);
void send_disconnect_packet(tmq_tcp_conn_t* conn, tmq_disconnect_pkt* pkt);

#endif //TINYMQTT_MQTT_CODEC_H
