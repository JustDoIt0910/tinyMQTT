//
// Created by zr on 23-6-2.
//

#ifndef TINYMQTT_MQTT_IO_GROUP_H
#define TINYMQTT_MQTT_IO_GROUP_H
#include "event/mqtt_event.h"
#include "mqtt_types.h"
//#include "3rd/fifo/queue.h"

#define MQTT_TCP_CHECKALIVE_INTERVAL    100
#define MQTT_CONNECT_MAX_PENDING        100
#define MQTT_TCP_MAX_IDLE               600

typedef tmq_map(char*, tmq_tcp_conn_t*) tcp_conn_map_t;
typedef tmq_vec(tmq_socket_t) tcp_conn_list;
typedef struct tmq_io_group_s
{
    tmq_broker_t* broker;
    pthread_t io_thread;
    tmq_event_loop_t loop;
    tcp_conn_map_t tcp_conns;
    tmq_timerid_t tcp_checkalive_timer;
    tmq_timerid_t mqtt_keepalive_timer;

    /* guarded by pending_conns_lk */
    tcp_conn_list pending_tcp_conns;
    /* guarded by connect_resp_lk */
    connect_resp_list connect_resps;
    /* guarded by sending_packets_lk */
    packet_send_list sending_packets;

    pthread_mutex_t pending_conns_lk;
    pthread_mutex_t connect_resp_lk;
    pthread_mutex_t sending_packets_lk;

    tmq_notifier_t new_conn_notifier;
    tmq_notifier_t connect_resp_notifier;
    tmq_notifier_t sending_packets_notifier;
} tmq_io_group_t;

void tmq_io_group_init(tmq_io_group_t* group, tmq_broker_t* broker);
void tmq_io_group_run(tmq_io_group_t* group);
void tmq_io_group_stop(tmq_io_group_t* group);

#endif //TINYMQTT_MQTT_IO_GROUP_H
