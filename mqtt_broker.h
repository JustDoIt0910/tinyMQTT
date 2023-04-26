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
    tmq_event_loop_t loop;
    tmq_acceptor_t acceptor;
    tcp_conn_map_t conn_map;
} tmq_broker_t;

#endif //TINYMQTT_MQTT_BROKER_H
