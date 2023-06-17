//
// Created by zr on 23-6-17.
//

#ifndef TINYMQTT_MQTT_CONNECTOR_H
#define TINYMQTT_MQTT_CONNECTOR_H
#include "base/mqtt_socket.h"
#include "event/mqtt_event.h"

typedef struct tmq_connector_s
{
    tmq_event_loop_t* loop;
    tmq_socket_addr_t server_addr;
} tmq_connector_t;

#endif //TINYMQTT_MQTT_CONNECTOR_H
