//
// Created by zr on 23-6-8.
//
#include "mqtt_types.h"

void tcp_conn_ctx_cleanup(void* arg)
{
    if(!arg) return;
    tcp_conn_ctx* ctx = arg;

    tmq_any_packet_t* pkt = tmq_vec_begin(ctx->pending_packets);
    for(; pkt != tmq_vec_end(ctx->pending_packets); pkt++)
        tmq_any_pkt_cleanup(pkt);
    tmq_vec_free(ctx->pending_packets);
}