//
// Created by zr on 23-4-9.
//

#ifndef TINYMQTT_MQTT_CLIENT_H
#define TINYMQTT_MQTT_CLIENT_H
#include "net/mqtt_tcp_conn.h"
#include "net/mqtt_connector.h"
#include "mqtt_session.h"
#include "mqtt_codec.h"

#define NETWORK_ERROR -1

typedef struct connect_options
{
    char* username, *password, *client_id;
    int clean_session;
    uint16_t keep_alive;
    char* will_message, *will_topic;
    uint8_t will_qos;
    int will_retain;
} connect_options;

typedef enum
{
    ASYNC_CONNECT,
    ASYNC_SUBSCRIBE,
    ASYNC_UNSUBSCRIBE,
    ASYNC_PUBLISH,
    ASYNC_DISCONNECT
} async_op_type_e;
typedef struct async_op_s
{
    async_op_type_e type;
    void* arg;
} async_op;

typedef struct tmq_client_s tiny_mqtt;
typedef void(*mqtt_message_cb)(char* topic, char* message, uint8_t qos, uint8_t retain);
typedef void(*mqtt_connect_cb)(tiny_mqtt* mqtt, int return_code);
typedef void(*mqtt_subscribe_cb)(tiny_mqtt* mqtt, sub_return_codes return_codes);
typedef void(*mqtt_unsubscribe_cb)(tiny_mqtt* mqtt);
typedef void(*mqtt_publish_cb)(tiny_mqtt* mqtt, uint16_t packet_id, uint8_t qos);
typedef void(*mqtt_disconnect_cb)(tiny_mqtt* mqtt);
typedef tmq_vec(async_op) async_op_list;
typedef struct tmq_client_s
{
    tmq_event_loop_t loop;
    tmq_tcp_conn_t* conn;
    tmq_session_t* session;
    tmq_codec_t codec;

    pthread_t io_thread;
    pthread_mutex_t lk;
    pthread_cond_t cond;
    int ready, async;
    async_op_list async_ops;
    tmq_notifier_t async_op_notifier;

    tmq_connector_t connector;
    connect_options connect_options;
    int connect_res;
    sub_return_codes subscribe_res;

    mqtt_message_cb on_message;
    mqtt_connect_cb on_connect;
    mqtt_subscribe_cb on_subscribe;
    mqtt_unsubscribe_cb on_unsubscribe;
    mqtt_publish_cb on_publish;
    mqtt_disconnect_cb on_disconnect;
} tiny_mqtt;

tiny_mqtt* tinymqtt_new(const char* ip, uint16_t port);
void tinymqtt_set_connect_callback(tiny_mqtt* mqtt, mqtt_connect_cb cb);
int tinymqtt_connect(tiny_mqtt* mqtt, connect_options* options);
void tinymqtt_set_subscribe_callback(tiny_mqtt* mqtt, mqtt_subscribe_cb cb);
int tinymqtt_subscribe(tiny_mqtt* mqtt, const char* topic_filter, uint8_t qos);
void tinymqtt_set_unsubscribe_callback(tiny_mqtt* mqtt, mqtt_unsubscribe_cb cb);
void tinymqtt_unsubscribe(tiny_mqtt* mqtt, const char* topic_filter);
void tinymqtt_set_message_callback(tiny_mqtt* mqtt, mqtt_message_cb cb);
void tinymqtt_set_publish_callback(tiny_mqtt* mqtt, mqtt_publish_cb cb);
void tinymqtt_publish(tiny_mqtt* mqtt, const char* topic, const char* message, uint8_t qos, int retain);
void tinymqtt_set_disconnect_callback(tiny_mqtt* mqtt, mqtt_disconnect_cb cb);
void tinymqtt_disconnect(tiny_mqtt* mqtt);

void tinymqtt_loop(tiny_mqtt* mqtt);
void tinymqtt_loop_threaded(tiny_mqtt* mqtt);
void tinymqtt_quit(tiny_mqtt* mqtt);
void tinymqtt_async_wait(tiny_mqtt* mqtt);
void tinymqtt_destroy(tiny_mqtt* mqtt);

#endif //TINYMQTT_MQTT_CLIENT_H
