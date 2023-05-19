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

void tmq_codec_init(tmq_codec_t* codec);

#endif //TINYMQTT_MQTT_CODEC_H
