//
// Created by zr on 23-6-17.
//

#ifndef TINYMQTT_MQTT_CONNECTOR_H
#define TINYMQTT_MQTT_CONNECTOR_H
#include "base/mqtt_socket.h"
#include "event/mqtt_event.h"
#define INITIAL_RETRY_INTERVAL 1.0

typedef void(*tcp_connected_cb)(void* arg, tmq_socket_t socket);
typedef void(*tcp_connect_failed_cb)(void* arg);

typedef struct tmq_connector_s
{
    tmq_event_loop_t* loop;
    tmq_socket_addr_t server_addr;
    tmq_event_handler_t* write_handler, *error_handler;
    tcp_connected_cb on_tcp_connect;
    tcp_connect_failed_cb on_connect_failed;
    void* cb_arg;
    double retry_interval;
    int max_retry;
    int retry_times;
} tmq_connector_t;

void tmq_connector_init(tmq_connector_t* connector, tmq_event_loop_t* loop, const char* server_ip, uint16_t server_port,
                        tcp_connected_cb on_connect, tcp_connect_failed_cb on_failed, void* cb_arg, int max_retry);
void tmq_connector_connect(tmq_connector_t* connector);

#endif //TINYMQTT_MQTT_CONNECTOR_H
