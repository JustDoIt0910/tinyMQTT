//
// Created by zr on 23-4-20.
//

#ifndef TINYMQTT_MQTT_PROTO_CODEC_H
#define TINYMQTT_MQTT_PROTO_CODEC_H
#include "mqtt/mqtt_packet.h"
#include "mqtt_codec.h"

typedef struct tmq_broker_s tmq_broker_t;
typedef struct tmq_client_s tiny_mqtt;
typedef struct tmq_session_s tmq_session_t;

typedef struct tmq_fixed_header
{
    uint8_t type_flags;
    uint32_t remain_length;
} tmq_fixed_header;

#define PACKET_TYPE(header) (((header).type_flags >> 4) & 0x0F)
#define FLAGS(header)       (((header).type_flags) & 0x0F)

typedef void (*connect_pkt_cb) (tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_connect_pkt* connect_pkt);
typedef void (*connack_pkt_cb) (tiny_mqtt* client, tmq_connack_pkt* connack_pkt);
typedef void (*publish_pkt_cb) (tmq_session_t* session, tmq_publish_pkt* publish_pkt);
typedef void (*puback_pkt_cb) (tmq_session_t* session, tmq_puback_pkt* puback_pkt);
typedef void (*pubrec_pkt_cb) (tmq_session_t* session, tmq_pubrec_pkt* pubrec_pkt);
typedef void (*pubrel_pkt_cb) (tmq_session_t* session, tmq_pubrel_pkt* pubrel_pkt);
typedef void (*pubcomp_pkt_cb) (tmq_session_t* session, tmq_pubcomp_pkt* pubcomp_pkt);
typedef void (*subscribe_pkt_cb) (tmq_session_t* session, tmq_subscribe_pkt* subscribe_pkt);
typedef void (*suback_pkt_cb) (tmq_session_t* session, tmq_suback_pkt* suback_pkt);
typedef void (*unsubscribe_pkt_cb) (tmq_session_t* session, tmq_unsubscribe_pkt* unsubscribe_pkt);
typedef void (*unsuback_pkt_cb) (tmq_session_t* session, tmq_unsuback_pkt* unsuback_pkt);
typedef void (*pingreq_pkt_cb) (tmq_session_t* session);
typedef void (*pingresp_pkt_cb) (tmq_session_t* session);
typedef void (*disconnect_pkt_cb) (tmq_broker_t* broker, tmq_session_t* session);

typedef struct tmq_mqtt_codec_s
{
    CODEC_PUBLIC_MEMBERS
    connect_pkt_cb on_connect;
    connack_pkt_cb on_conn_ack;

    publish_pkt_cb on_publish;
    puback_pkt_cb on_pub_ack;
    pubrec_pkt_cb on_pub_rec;
    pubrel_pkt_cb on_pub_rel;
    pubcomp_pkt_cb on_pub_comp;

    subscribe_pkt_cb on_subscribe;
    suback_pkt_cb on_sub_ack;
    unsubscribe_pkt_cb on_unsubscribe;
    unsuback_pkt_cb on_unsub_ack;

    pingreq_pkt_cb on_ping_req;
    pingresp_pkt_cb on_ping_resp;

    disconnect_pkt_cb on_disconnect;
} tmq_mqtt_codec_t;

typedef enum mqtt_decode_status_e
{
    DECODE_OK,
    NEED_MORE_DATA,
    UNKNOWN_PACKET,
    BAD_PACKET_FORMAT,
    PROTOCOL_ERROR,
    UNEXPECTED_PACKET
} mqtt_decode_status;

typedef enum mqtt_parsing_state_e
{
    PARSING_FIXED_HEADER,
    PARSING_REMAIN_LENGTH,
    PARSING_BODY
} mqtt_parsing_state;

typedef struct mqtt_parsing_ctx_s
{
    mqtt_parsing_state state;
    tmq_fixed_header fixed_header;
    /* save the current multiplier when decoding remain_length */
    uint32_t multiplier;
} mqtt_parsing_ctx_t;

void tmq_mqtt_codec_init(tmq_mqtt_codec_t* codec, tmq_codec_type type);

void send_connect_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_conn_ack_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_publish_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_pub_ack_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_pub_rec_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_pub_rel_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_pub_comp_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_subscribe_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_sub_ack_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_unsubscribe_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_unsub_ack_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_ping_req_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_ping_resp_packet(tmq_tcp_conn_t* conn, void* pkt);
void send_disconnect_packet(tmq_tcp_conn_t* conn, void* pkt);

void tmq_send_any_packet(tmq_tcp_conn_t* conn, tmq_any_packet_t* pkt);

#endif //TINYMQTT_MQTT_PROTO_CODEC_H
