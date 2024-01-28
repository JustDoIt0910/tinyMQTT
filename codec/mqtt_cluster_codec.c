//
// Created by just do it on 2024/1/25.
//
#include <string.h>
#include <malloc.h>
#include "mqtt_cluster_codec.h"
#include "net/mqtt_buffer.h"
#include "net/mqtt_tcp_conn.h"
#include "mqtt/mqtt_types.h"
#include "stdio.h"

static void parse_add_route_message(tmq_len_based_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer, uint16_t len)
{
    tmq_cluster_codec_t* cluster_codec = (tmq_cluster_codec_t*)codec;
    tmq_str_t topic_filter = tmq_str_new_len(NULL, len);
    tmq_buffer_read(buffer, topic_filter, len);
    tcp_conn_simple_ctx_t* ctx = conn->context;
    cluster_codec->on_add_route(ctx->broker, conn, topic_filter);
}

static void parse_tun_publish_message(tmq_len_based_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer, uint16_t len)
{
    tmq_cluster_codec_t* cluster_codec = (tmq_cluster_codec_t*)codec;
    /* use mqtt codec to decode the tunneled mqtt publish packet */
    /* create a temporary mqtt conn context */
    tcp_conn_mqtt_ctx_t* tmp_mqtt_ctx = malloc(sizeof(tcp_conn_mqtt_ctx_t));
    tmp_mqtt_ctx->upstream.broker = cluster_codec->broker;
    tmp_mqtt_ctx->conn_state = TUNNELED;
    tmp_mqtt_ctx->parsing_ctx.state = PARSING_FIXED_HEADER;
    tcp_conn_simple_ctx_t* cur_conn_ctx = conn->context;
    conn->context = tmp_mqtt_ctx;
    cluster_codec->mqtt_codec->decode_tcp_message((tmq_codec_t*)cluster_codec, conn, buffer);
    conn->context = cur_conn_ctx;
    free(tmp_mqtt_ctx);
}

extern void add_route_message_handler(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_str_t topic_filter);
extern void receive_tunnel_publish_handler(tmq_broker_t* broker, tmq_publish_pkt* publish_pkt);
void tmq_cluster_codec_init(tmq_cluster_codec_t* codec, tmq_mqtt_codec_t* mqtt_codec, tmq_broker_t* broker)
{
    tmq_len_based_codec_init((tmq_len_based_codec_t*)codec);
    tmq_len_based_codec_register_parser((tmq_len_based_codec_t*)codec, CLUSTER_ROUTE_SYNC, parse_add_route_message);
    tmq_len_based_codec_register_parser((tmq_len_based_codec_t*)codec, CLUSTER_TUN_PUBLISH, parse_tun_publish_message);
    codec->type = SERVER_CODEC;
    codec->broker = broker;
    codec->mqtt_codec = mqtt_codec;
    codec->on_add_route = add_route_message_handler;
    codec->on_tun_publish = receive_tunnel_publish_handler;
}

extern void pack_uint16(packet_buf* buf, uint16_t value);
extern int pack_publish_packet(packet_buf* buf, tmq_publish_pkt* publish_pkt);

void send_route_sync_del_message(tmq_tcp_conn_t* conn, cluster_message_type type, tmq_str_t payload)
{
    packet_buf buf = tmq_vec_make(uint8_t);
    tmq_vec_push_back(buf, type);
    uint16_t payload_len =  tmq_str_len(payload) + 1;
    pack_uint16(&buf, payload_len);
    tmq_tcp_conn_write(conn, (char*) tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
    tmq_tcp_conn_write(conn, payload, payload_len);
}

void send_publish_tun_message(tmq_tcp_conn_t* conn, tmq_str_t topic, mqtt_message* message, uint8_t retain)
{
    tmq_publish_pkt publish_pkt = {
            .topic = topic,
            .payload = message->message,
            .packet_id = 0,
            .flags = (message->qos << 1) | retain
    };
    packet_buf buf = tmq_vec_make(uint8_t);
    tmq_vec_push_back(buf, CLUSTER_TUN_PUBLISH);
    pack_uint16(&buf, 0);
    pack_publish_packet(&buf, &publish_pkt);
    uint16_t payload_len = tmq_vec_size(buf) - LEN_BASED_HEADER_SIZE;
    *(uint16_t*)tmq_vec_at(buf, 1) = htobe16(payload_len);
    tmq_tcp_conn_write(conn, (char*)tmq_vec_begin(buf), tmq_vec_size(buf));
    tmq_vec_free(buf);
}