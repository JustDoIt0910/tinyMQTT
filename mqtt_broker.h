//
// Created by zr on 23-4-9.
//

#ifndef TINYMQTT_MQTT_BROKER_H
#define TINYMQTT_MQTT_BROKER_H
#include "mqtt_event.h"
#include "mqtt_acceptor.h"
#include "mqtt_tcp_conn.h"
#include "mqtt_map.h"
#include "mqtt_codec.h"
#define MQTT_CONNECT_PENDING

typedef tmq_map(tmq_tcp_conn_t*, tmq_timer_t*) pending_conn_list;

typedef struct tmq_broker_s
{
    tmq_event_loop_t event_loop;
    tmq_acceptor_t acceptor;
    pthread_mutex_t conn_lk;
    pending_conn_list pending_conns;
    connect_pkt_codec codec;
} tmq_broker_t;

void tmq_broker_init(tmq_broker_t* broker, uint16_t port);
void tmq_broker_run(tmq_broker_t* broker);

#endif //TINYMQTT_MQTT_BROKER_H
