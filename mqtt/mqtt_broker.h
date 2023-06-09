//
// Created by zr on 23-4-9.
//

#ifndef TINYMQTT_MQTT_BROKER_H
#define TINYMQTT_MQTT_BROKER_H
#include "event/mqtt_event.h"
#include "net/mqtt_acceptor.h"
#include "base/mqtt_str.h"
#include "base/mqtt_map.h"
#include "base/mqtt_config.h"
#include "mqtt_codec.h"
#include "mqtt_io_group.h"
#include "mqtt_topic.h"
#include "mqtt_types.h"

#define MQTT_IO_THREAD  4

typedef tmq_map(char*, tmq_session_t*) tmq_session_map;
typedef struct tmq_broker_s
{
    tmq_event_loop_t loop;
    tmq_acceptor_t acceptor;
    tmq_codec_t codec;
    int next_io_group;
    tmq_io_group_t io_groups[MQTT_IO_THREAD];
    tmq_config_t conf, pwd_conf;
    tmq_session_map sessions;
    tmq_topics_t topics_tree;

    /* guarded by session_ctl_lk */
    session_ctl_list session_ctl_reqs;
    /* guarded by message_ctl_lk */
    message_ctl_list message_ctl_reqs;

    pthread_mutex_t session_ctl_lk;
    pthread_mutex_t message_ctl_lk;

    tmq_notifier_t session_ctl_notifier;
    tmq_notifier_t message_ctl_notifier;
} tmq_broker_t;

int tmq_broker_init(tmq_broker_t* broker, const char* cfg);
void tmq_broker_run(tmq_broker_t* broker);

#endif //TINYMQTT_MQTT_BROKER_H
