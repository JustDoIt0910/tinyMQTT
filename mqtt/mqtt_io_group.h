//
// Created by zr on 23-6-2.
//

#ifndef TINYMQTT_MQTT_IO_GROUP_H
#define TINYMQTT_MQTT_IO_GROUP_H
#include "event/mqtt_event.h"
#include "mqtt_def.h"

typedef tmq_map(char*, tmq_tcp_conn_t*) tcp_conn_map_t;
typedef struct tmq_io_group_s
{
    tmq_broker_t* broker;
    pthread_t io_thread;
    tmq_event_loop_t loop;
    tcp_conn_map_t tcp_conns;
    tmq_timerid_t tcp_checkalive_timer;

    tmq_notifier_t new_conn_notifier;
    pthread_mutex_t pending_conns_lk;
    /* guarded by pending_conns_lk */
    tmq_vec(tmq_socket_t) pending_conns;

    pthread_mutex_t connect_resp_lk;
    /* guarded by connect_resp_lk */
    connect_resp_list connect_resp;
    tmq_notifier_t connect_resp_notifier;
} tmq_io_group_t;

void tmq_io_group_init(tmq_io_group_t* group, tmq_broker_t* broker);
void tmq_io_group_run(tmq_io_group_t* group);
void tmq_io_group_stop(tmq_io_group_t* group);

#endif //TINYMQTT_MQTT_IO_GROUP_H
