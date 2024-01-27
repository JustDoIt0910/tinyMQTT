//
// Created by just do it on 2024/1/25.
//
#include <string.h>
#include "mqtt_cluster_codec.h"
#include "cluster/mqtt_cluster.h"
#include "net/mqtt_buffer.h"
#include "net/mqtt_tcp_conn.h"
#include "mqtt/mqtt_types.h"

static void parse_add_router_message(tmq_len_based_codec_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer, uint16_t len)
{
    tmq_cluster_codec_t* cluster_codec = (tmq_cluster_codec_t*)codec;
    tmq_str_t topic_filter = tmq_str_new_len(NULL, len);
    tmq_buffer_read(buffer, topic_filter, len);
    tcp_conn_simple_ctx_t* ctx = conn->context;
    cluster_codec->on_add_route(ctx->broker, conn, topic_filter);
}

extern void add_route_message_handler(tmq_broker_t* broker, tmq_tcp_conn_t* conn, tmq_str_t topic_filter);
void tmq_cluster_codec_init(tmq_cluster_codec_t* codec)
{
    tmq_len_based_codec_init((tmq_len_based_codec_t*)codec);
    tmq_len_based_codec_register_parser((tmq_len_based_codec_t*)codec, CLUSTER_ROUTE_SYNC, parse_add_router_message);
    codec->type = SERVER_CODEC;
    codec->on_add_route = add_route_message_handler;
}

extern void pack_uint16(packet_buf* buf, uint16_t value);

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