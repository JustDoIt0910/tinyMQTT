//
// Created by zr on 23-4-9.
//

#ifndef TINYMQTT_MQTT_BROKER_H
#define TINYMQTT_MQTT_BROKER_H
#include "mqtt_event.h"
#include "mqtt_acceptor.h"
#include "mqtt_map.h"
#include "mqtt_codec.h"

#define MQTT_ALIVE_TIMER_INTERVAL   10 * 1000
#define MQTT_CONNECT_PENDING        30
#define MQTT_IO_THREAD              4

typedef struct
{
    tmq_timer_t* timer;
    int ttl;
} active_ctx;
typedef tmq_map(tmq_tcp_conn_t*, active_ctx) tcp_conn_list;
typedef struct tmq_broker_s tmq_broker_t;

typedef struct
{
    void* context;
    int in_session;
} tcp_conn_ctx;

typedef struct tmq_io_group_s
{
    tmq_broker_t* broker;
    pthread_t io_thread;
    tmq_event_loop_t loop;
    tcp_conn_list tcp_conns;

    int wakeup_pipe[2];
    tmq_event_handler_t* wakeup_handler;
    pthread_mutex_t pending_conns_lk;
    tmq_vec(tmq_socket_t) pending_conns;
} tmq_io_group_t;

typedef struct tmq_broker_s
{
    tmq_event_loop_t event_loop;
    tmq_acceptor_t acceptor;
    tmq_codec_t codec;
    int next_io_group;
    tmq_io_group_t io_groups[MQTT_IO_THREAD];
} tmq_broker_t;

void tmq_broker_init(tmq_broker_t* broker, uint16_t port);
void tmq_broker_run(tmq_broker_t* broker);

#endif //TINYMQTT_MQTT_BROKER_H
