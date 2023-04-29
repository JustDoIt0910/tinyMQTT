//
// Created by zr on 23-4-9.
//

#ifndef TINYMQTT_MQTT_BROKER_H
#define TINYMQTT_MQTT_BROKER_H
#include "mqtt_event.h"
#include "mqtt_acceptor.h"
#include "mqtt_tcp_conn.h"
#include "mqtt_map.h"

typedef tmq_map(const char*, tmq_tcp_conn_t*) tcp_conn_map_t;

typedef struct tmq_broker_s
{
    tmq_event_loop_t event_loop;
    tmq_acceptor_t acceptor;
    pthread_mutex_t conn_map_lk;
    tcp_conn_map_t conn_map;
} tmq_broker_t;

void tmq_broker_init(tmq_broker_t* broker, uint16_t port);
void tmq_broker_run(tmq_broker_t* broker);

#endif //TINYMQTT_MQTT_BROKER_H
