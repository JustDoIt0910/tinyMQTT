//
// Created by zr on 23-4-9.
//
#include "mqtt_client.h"
#include <stdlib.h>
#include <string.h>

static void on_tcp_connected(void* arg, tmq_socket_t sock)
{
    tiny_mqtt* mqtt = arg;
    mqtt->conn = get_ref(tmq_tcp_conn_new(&mqtt->loop, NULL, sock, &mqtt->codec));
    tcp_conn_ctx* conn_ctx = malloc(sizeof(tcp_conn_ctx));
    conn_ctx->upstream.client = mqtt;
    conn_ctx->conn_state = NO_SESSION;
    conn_ctx->parsing_ctx.state = PARSING_FIXED_HEADER;
    conn_ctx->last_msg_time = time_now();
    tmq_tcp_conn_set_context(mqtt->conn, conn_ctx, NULL);

    tmq_connect_pkt pkt;
    bzero(&pkt, sizeof(tmq_connect_pkt));
    if(mqtt->connect_ops.clean_session) pkt.flags |= 0x02;
    if(mqtt->connect_ops.will_message)
    {
        pkt.flags |= 0x04;
        pkt.flags |= (mqtt->connect_ops.will_qos << 3);
        if(mqtt->connect_ops.will_retain) pkt.flags |= 0x20;
        pkt.will_message = tmq_str_new(mqtt->connect_ops.will_message);
        pkt.will_topic = tmq_str_new(mqtt->connect_ops.will_topic);
    }
    if(mqtt->connect_ops.username)
    {
        pkt.flags |= 0x80;
        pkt.username = tmq_str_new(mqtt->connect_ops.username);
    }
    if(mqtt->connect_ops.password)
    {
        pkt.flags |= 0x40;
        pkt.password = tmq_str_new(mqtt->connect_ops.password);
    }
    pkt.client_id = tmq_str_new(mqtt->connect_ops.client_id);
    pkt.keep_alive = mqtt->connect_ops.keep_alive;
    send_connect_packet(mqtt->conn, &pkt);
}

static void on_tcp_connect_failed(void* arg)
{
    tiny_mqtt* mqtt = arg;
    mqtt->connect_res = NETWORK_ERROR;
    tmq_event_loop_quit(&mqtt->loop);
}

static void on_mqtt_message(void* arg, char* topic, tmq_message* message, uint8_t retain)
{
    tiny_mqtt* mqtt = arg;
    if(mqtt->on_message)
        mqtt->on_message(topic, message->message, message->qos, retain);
    tmq_str_free(message->message);
}

static void on_publish_finish(void* arg)
{
    tiny_mqtt* mqtt = arg;
    tmq_event_loop_quit(&mqtt->loop);
}

void on_mqtt_connect_response(tiny_mqtt* mqtt, tmq_connack_pkt* connack_pkt)
{
    mqtt->connect_res = connack_pkt->return_code;
    tcp_conn_ctx* ctx = mqtt->conn->context;
    ctx->conn_state = IN_SESSION;
    mqtt->session = tmq_session_new(mqtt, on_mqtt_message, NULL, mqtt->conn, mqtt->connect_ops.client_id,
                                    mqtt->connect_ops.clean_session, mqtt->connect_ops.keep_alive,
                                    mqtt->connect_ops.will_topic, mqtt->connect_ops.will_message,
                                    mqtt->connect_ops.will_qos, mqtt->connect_ops.will_retain, 1);
    tmq_session_set_publish_finish_callback(mqtt->session, on_publish_finish);
    ctx->upstream.session = mqtt->session;
    tmq_event_loop_quit(&mqtt->loop);
}

void on_mqtt_subscribe_response(tiny_mqtt* mqtt, tmq_suback_pkt * suback_pkt)
{
    mqtt->subscribe_res = suback_pkt->return_codes;
    tmq_event_loop_quit(&mqtt->loop);
}

tiny_mqtt* tinymqtt_new(const char* ip, uint16_t port)
{
    tiny_mqtt* mqtt = malloc(sizeof(tiny_mqtt));
    if(!mqtt) return NULL;
    bzero(mqtt, sizeof(tiny_mqtt));
    tmq_event_loop_init(&mqtt->loop);
    tmq_codec_init(&mqtt->codec, CLIENT_CODEC);
    tmq_connector_init(&mqtt->connector, &mqtt->loop, ip, port, on_tcp_connected, on_tcp_connect_failed, mqtt, 3);
    return mqtt;
}

int tinymqtt_connect(tiny_mqtt* mqtt, connect_options* options)
{
    mqtt->connect_ops = *options;
    tmq_connector_connect(&mqtt->connector);
    tmq_event_loop_run(&mqtt->loop);
    return mqtt->connect_res;
}

int tinymqtt_subscribe(tiny_mqtt* mqtt, const char* topic_filter, uint8_t qos)
{
    if(!mqtt->session) return -1;
    tmq_session_subscribe(mqtt->session, topic_filter, qos);
    tmq_event_loop_run(&mqtt->loop);
    int ret = -1;
    if(tmq_vec_size(mqtt->subscribe_res) > 0)
        ret = *tmq_vec_at(mqtt->subscribe_res, 0);
    tmq_vec_free(mqtt->subscribe_res);
    return ret;
}

void tinymqtt_publish(tiny_mqtt* mqtt, const char* topic, const char* message, uint8_t qos, int retain)
{
    if(!message || !topic || qos > 2 || !mqtt->session) return;
    tmq_session_publish(mqtt->session, topic, message, qos, retain);
    tmq_event_loop_run(&mqtt->loop);
}

void tinymqtt_set_message_callback(tiny_mqtt* mqtt, mqtt_message_cb cb)
{
    if(!mqtt) return;
    mqtt->on_message = cb;
}

void tinymqtt_loop(tiny_mqtt* mqtt){tmq_event_loop_run(&mqtt->loop);}