//
// Created by zr on 23-4-9.
//
#include "mqtt_client.h"
#include "tlog.h"
#include <stdlib.h>
#include <string.h>

struct publish_args
{
    tmq_str_t message;
    tmq_str_t topic;
    uint8_t qos;
    int retain;
};

void tcp_conn_close_cb(tmq_tcp_conn_t* conn, void* arg)
{
    tiny_mqtt* mqtt = arg;
    release_ref(mqtt->conn);
    mqtt->conn = NULL;
    tcp_conn_ctx* ctx = conn->context;
    if(ctx->conn_state == IN_SESSION)
        tmq_session_close(ctx->upstream.session);
}

static void on_tcp_connected(void* arg, tmq_socket_t sock)
{
    tiny_mqtt* mqtt = arg;
    mqtt->conn = get_ref(tmq_tcp_conn_new(&mqtt->loop, NULL, sock, &mqtt->codec));
    mqtt->conn->on_close = tcp_conn_close_cb;
    mqtt->conn->cb_arg = mqtt;

    tcp_conn_ctx* conn_ctx = malloc(sizeof(tcp_conn_ctx));
    conn_ctx->upstream.client = mqtt;
    conn_ctx->conn_state = NO_SESSION;
    conn_ctx->parsing_ctx.state = PARSING_FIXED_HEADER;
    conn_ctx->last_msg_time = time_now();
    tmq_tcp_conn_set_context(mqtt->conn, conn_ctx, NULL);

    tmq_connect_pkt pkt;
    bzero(&pkt, sizeof(tmq_connect_pkt));
    if(mqtt->connect_options.clean_session) pkt.flags |= 0x02;
    if(mqtt->connect_options.will_message)
    {
        pkt.flags |= 0x04;
        pkt.flags |= (mqtt->connect_options.will_qos << 3);
        if(mqtt->connect_options.will_retain) pkt.flags |= 0x20;
        pkt.will_message = tmq_str_new(mqtt->connect_options.will_message);
        pkt.will_topic = tmq_str_new(mqtt->connect_options.will_topic);
    }
    if(mqtt->connect_options.username)
    {
        pkt.flags |= 0x80;
        pkt.username = tmq_str_new(mqtt->connect_options.username);
    }
    if(mqtt->connect_options.password)
    {
        pkt.flags |= 0x40;
        pkt.password = tmq_str_new(mqtt->connect_options.password);
    }
    pkt.client_id = tmq_str_new(mqtt->connect_options.client_id);
    pkt.keep_alive = mqtt->connect_options.keep_alive;
    send_connect_packet(mqtt->conn, &pkt);
}

static void on_tcp_connect_failed(void* arg)
{
    tiny_mqtt* mqtt = arg;
    mqtt->connect_res = NETWORK_ERROR;
    if(mqtt->async)
    {
        if(mqtt->on_disconnect)
            mqtt->on_disconnect(mqtt);
    }
    else tmq_event_loop_quit(&mqtt->loop);
}

static void on_mqtt_message(void* arg, char* topic, tmq_message* message, uint8_t retain)
{
    tiny_mqtt* mqtt = arg;
    if(mqtt->on_message)
        mqtt->on_message(topic, message->message, message->qos, retain);
    tmq_str_free(message->message);
}

static void on_publish_finish(void* arg, uint16_t packet_id, uint8_t qos)
{
    tiny_mqtt* mqtt = arg;
    if(mqtt->async)
    {
        if(mqtt->on_publish)
            mqtt->on_publish(mqtt, packet_id, qos);
    }
    else tmq_event_loop_quit(&mqtt->loop);
}

static void on_disconnected(void* arg)
{
    tiny_mqtt* mqtt = arg;
    if(mqtt->async)
    {
        tmq_tcp_conn_close(mqtt->conn);
        if(mqtt->on_disconnect)
            mqtt->on_disconnect(mqtt);
    }
    else tmq_event_loop_quit(&mqtt->loop);
}

void on_mqtt_connect_response(tiny_mqtt* mqtt, tmq_connack_pkt* connack_pkt)
{
    mqtt->connect_res = connack_pkt->return_code;
    tcp_conn_ctx* ctx = mqtt->conn->context;
    ctx->conn_state = IN_SESSION;
    if(!mqtt->session)
        mqtt->session = tmq_session_new(mqtt, on_mqtt_message, NULL, mqtt->conn, mqtt->connect_options.client_id,
                                        mqtt->connect_options.clean_session, mqtt->connect_options.keep_alive,
                                        mqtt->connect_options.will_topic, mqtt->connect_options.will_message,
                                        mqtt->connect_options.will_qos, mqtt->connect_options.will_retain, 1);
    else
    {
        mqtt->session->state = OPEN;
        mqtt->session->conn = mqtt->conn;
    }
    tmq_session_set_publish_finish_callback(mqtt->session, on_publish_finish);
    ctx->upstream.session = mqtt->session;
    if(mqtt->async)
    {
        if(mqtt->on_connect)
            mqtt->on_connect(mqtt, connack_pkt->return_code);
    }
    else tmq_event_loop_quit(&mqtt->loop);
}

void on_mqtt_subscribe_response(tiny_mqtt* mqtt, tmq_suback_pkt* suback_pkt)
{
    mqtt->subscribe_res = suback_pkt->return_codes;
    if(mqtt->async)
    {
        if(mqtt->on_subscribe)
            mqtt->on_subscribe(mqtt, mqtt->subscribe_res);
        tmq_vec_free(mqtt->subscribe_res);
    }
    else tmq_event_loop_quit(&mqtt->loop);
}

void on_mqtt_unsubscribe_response(tiny_mqtt* mqtt, tmq_unsuback_pkt* unsuback_pkt)
{
    if(mqtt->async)
    {
        if(mqtt->on_unsubscribe)
            mqtt->on_unsubscribe(mqtt);
    }
    else tmq_event_loop_quit(&mqtt->loop);
}

static void handle_async_operations(void* arg)
{
    tiny_mqtt* mqtt = arg;
    pthread_mutex_lock(&mqtt->lk);
    for(async_op* op = tmq_vec_begin(mqtt->async_ops); op != tmq_vec_end(mqtt->async_ops); op++)
    {
        if(op->type == ASYNC_CONNECT)
            tmq_connector_connect(&mqtt->connector);
        else if(op->type == ASYNC_SUBSCRIBE)
        {
            topic_filter_qos* tf = op->arg;
            tmq_session_subscribe(mqtt->session, tf->topic_filter, tf->qos);
            tmq_str_free(tf->topic_filter);
            free(tf);
        }
        else if(op->type == ASYNC_UNSUBSCRIBE)
        {
            tmq_str_t tf = op->arg;
            tmq_session_unsubscribe(mqtt->session, tf);
            tmq_str_free(tf);
        }
        else if(op->type == ASYNC_PUBLISH)
        {
            struct publish_args* args = op->arg;
            tmq_session_publish(mqtt->session, args->topic, args->message, args->qos, args->retain);
            tmq_str_free(args->message);
            tmq_str_free(args->topic);
            free(args);
        }
        else
        {
            mqtt->conn->on_write_complete = on_disconnected;
            mqtt->conn->cb_arg = mqtt;
            send_disconnect_packet(mqtt->conn, NULL);
        }
    }
    tmq_vec_clear(mqtt->async_ops);
    pthread_mutex_unlock(&mqtt->lk);
}

tiny_mqtt* tinymqtt_new(const char* ip, uint16_t port)
{
    tiny_mqtt* mqtt = malloc(sizeof(tiny_mqtt));
    if(!mqtt) return NULL;
    bzero(mqtt, sizeof(tiny_mqtt));
    tmq_event_loop_init(&mqtt->loop);
    tmq_codec_init(&mqtt->codec, CLIENT_CODEC);
    tmq_connector_init(&mqtt->connector, &mqtt->loop, ip, port, on_tcp_connected, on_tcp_connect_failed, mqtt, 3);
    mqtt->ready = 0;
    mqtt->async = 0;
    pthread_mutex_init(&mqtt->lk, NULL);
    pthread_cond_init(&mqtt->cond, NULL);
    tmq_vec_init(&mqtt->async_ops, async_op);
    tmq_notifier_init(&mqtt->async_op_notifier, &mqtt->loop, handle_async_operations, mqtt);
    return mqtt;
}

void tinymqtt_set_connect_callback(tiny_mqtt* mqtt, mqtt_connect_cb cb) {if(mqtt) mqtt->on_connect = cb;}

int tinymqtt_connect(tiny_mqtt* mqtt, connect_options* options)
{
    mqtt->connect_options = *options;
    if(mqtt->async)
    {
        async_op op = {
                .type = ASYNC_CONNECT
        };
        pthread_mutex_lock(&mqtt->lk);
        tmq_vec_push_back(mqtt->async_ops, op);
        pthread_mutex_unlock(&mqtt->lk);
        tmq_notifier_notify(&mqtt->async_op_notifier);
        return 0;
    }
    tmq_connector_connect(&mqtt->connector);
    tmq_event_loop_run(&mqtt->loop);
    return mqtt->connect_res;
}

void tinymqtt_set_subscribe_callback(tiny_mqtt* mqtt, mqtt_subscribe_cb cb) {if(mqtt) mqtt->on_subscribe = cb;}

int tinymqtt_subscribe(tiny_mqtt* mqtt, const char* topic_filter, uint8_t qos)
{
    if(!mqtt->session) return -1;
    if(mqtt->async)
    {
        topic_filter_qos* tf = malloc(sizeof(topic_filter_qos));
        tf->topic_filter = tmq_str_new(topic_filter);
        tf->qos = qos;
        async_op op = {
                .type = ASYNC_SUBSCRIBE,
                .arg = tf
        };
        pthread_mutex_lock(&mqtt->lk);
        tmq_vec_push_back(mqtt->async_ops, op);
        pthread_mutex_unlock(&mqtt->lk);
        tmq_notifier_notify(&mqtt->async_op_notifier);
        return 0;
    }
    tmq_session_subscribe(mqtt->session, topic_filter, qos);
    tmq_event_loop_run(&mqtt->loop);
    int ret = -1;
    if(tmq_vec_size(mqtt->subscribe_res) > 0)
        ret = *tmq_vec_at(mqtt->subscribe_res, 0);
    tmq_vec_free(mqtt->subscribe_res);
    return ret;
}

void tinymqtt_set_unsubscribe_callback(tiny_mqtt* mqtt, mqtt_unsubscribe_cb cb) {if(mqtt) mqtt->on_unsubscribe = cb;}

void tinymqtt_unsubscribe(tiny_mqtt* mqtt, const char* topic_filter)
{
    if(!mqtt->session) return;
    if(mqtt->async)
    {
        tmq_str_t tf = tmq_str_new(topic_filter);
        async_op op = {
                .type = ASYNC_UNSUBSCRIBE,
                .arg = tf
        };
        pthread_mutex_lock(&mqtt->lk);
        tmq_vec_push_back(mqtt->async_ops, op);
        pthread_mutex_unlock(&mqtt->lk);
        tmq_notifier_notify(&mqtt->async_op_notifier);
        return;
    }
    tmq_session_unsubscribe(mqtt->session, topic_filter);
    tmq_event_loop_run(&mqtt->loop);
}

void tinymqtt_set_publish_callback(tiny_mqtt* mqtt, mqtt_publish_cb cb) {if(mqtt)mqtt->on_publish = cb;}

void tinymqtt_publish(tiny_mqtt* mqtt, const char* topic, const char* message, uint8_t qos, int retain)
{
    if(!message || !topic || qos > 2 || !mqtt->session) return;
    if(mqtt->async)
    {
        struct publish_args* args = malloc(sizeof(struct publish_args));
        args->message = tmq_str_new(message);
        args->topic = tmq_str_new(topic);
        args->qos = qos;
        args->retain = retain;
        async_op op = {
                .type = ASYNC_PUBLISH,
                .arg = args
        };
        pthread_mutex_lock(&mqtt->lk);
        tmq_vec_push_back(mqtt->async_ops, op);
        pthread_mutex_unlock(&mqtt->lk);
        tmq_notifier_notify(&mqtt->async_op_notifier);
        return;
    }
    tmq_session_publish(mqtt->session, topic, message, qos, retain);
    tmq_event_loop_run(&mqtt->loop);
}

void tinymqtt_set_message_callback(tiny_mqtt* mqtt, mqtt_message_cb cb) {if(mqtt) mqtt->on_message = cb;}

static void ping(void* arg)
{
    tiny_mqtt* mqtt = arg;
    int64_t now = time_now();
    if(now - mqtt->session->last_pkt_ts > 2 * SEC_US(mqtt->connect_options.keep_alive))
    {
        tlog_info("can not receive ping respond from server");
        if(mqtt->async)
        {
            tmq_tcp_conn_close(mqtt->conn);
            if(mqtt->on_disconnect)
                mqtt->on_disconnect(mqtt);
        }
        else tmq_event_loop_quit(&mqtt->loop);
        return;
    }
    send_pingreq_packet(mqtt->conn, NULL);
}

void tinymqtt_set_disconnect_callback(tiny_mqtt* mqtt, mqtt_disconnect_cb cb) {if(mqtt) mqtt->on_disconnect = cb;}

void tinymqtt_disconnect(tiny_mqtt* mqtt)
{
    if(mqtt->async)
    {
        async_op op = {
                .type = ASYNC_DISCONNECT,
        };
        pthread_mutex_lock(&mqtt->lk);
        tmq_vec_push_back(mqtt->async_ops, op);
        pthread_mutex_unlock(&mqtt->lk);
        tmq_notifier_notify(&mqtt->async_op_notifier);
        return;
    }
    mqtt->conn->on_write_complete = on_disconnected;
    mqtt->conn->cb_arg = mqtt;
    send_disconnect_packet(mqtt->conn, NULL);
    if(!mqtt->loop.quit)
        tmq_event_loop_run(&mqtt->loop);
    tmq_tcp_conn_close(mqtt->conn);
    if(mqtt->session->clean_session)
        tmq_session_free(mqtt->session);
}

void tinymqtt_loop(tiny_mqtt* mqtt)
{
    if(mqtt->connect_options.keep_alive > 0)
    {
        tmq_timer_t* ping_timer = tmq_timer_new(SEC_MS(mqtt->connect_options.keep_alive), 1, ping, mqtt);
        tmq_event_loop_add_timer(&mqtt->loop, ping_timer);
    }
    tmq_event_loop_run(&mqtt->loop);
}

static void* io_thread_func(void* arg)
{
    tiny_mqtt* mqtt = arg;
    pthread_mutex_lock(&mqtt->lk);
    mqtt->ready = 1;
    mqtt->async = 1;
    pthread_cond_signal(&mqtt->cond);
    pthread_mutex_unlock(&mqtt->lk);
    tinymqtt_loop(mqtt);
    tmq_event_loop_destroy(&mqtt->loop);
    return NULL;
}

void tinymqtt_loop_threaded(tiny_mqtt* mqtt)
{
    pthread_create(&mqtt->io_thread, NULL, io_thread_func, mqtt);
    pthread_mutex_lock(&mqtt->lk);
    while(!mqtt->ready)
        pthread_cond_wait(&mqtt->cond, &mqtt->lk);
    pthread_mutex_unlock(&mqtt->lk);
}