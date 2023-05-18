//
// Created by zr on 23-4-20.
//
#include "mqtt_codec.h"
#include "mqtt_event.h"
#include "mqtt_tcp_conn.h"
#include "tlog.h"
#include <stdlib.h>

static void decode_connect_pkt(tmq_codec_interface_t* codec, tmq_tcp_conn_t* conn, tmq_buffer_t* buffer)
{
    tlog_info("decode_connect_pkt");
}

void connect_msg_codec_init(connect_pkt_codec* codec, tmq_broker_t* broker, tmq_on_connect_cb on_connect)
{
    if(!codec) return;
    codec->broker = broker;
    codec->tmq_on_connect = on_connect;
    codec->on_message = decode_connect_pkt;
}