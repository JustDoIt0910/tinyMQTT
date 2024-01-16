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
#include "mqtt_io_context.h"
#include "mqtt_topic.h"
#include "mqtt_types.h"
#include "mqtt_executor.h"
#include "mqtt_acl.h"
#include "db/mqtt_conn_pool.h"
#include "thrdpool/thrdpool.h"
#include <mongoc/mongoc.h>

#define DEFAULT_IO_THREADS  4

typedef tmq_map(char*, tmq_session_t*) tmq_session_map;
typedef struct tmq_broker_s
{
    tmq_event_loop_t loop;
    tmq_acceptor_t acceptor;
    tmq_codec_t codec;
    tmq_executor_t executor;
    tmq_config_t conf, pwd_conf;
    tmq_acl_t acl;
    tmq_mysql_conn_pool_t mysql_pool;
    mongoc_client_pool_t* mongodb_pool;
    thrdpool_t* thread_pool;
    tmq_io_context_t* io_contexts;
    int next_io_context;
    tmq_session_map sessions;
    tmq_topics_t topics_tree;
    uint8_t inflight_window_size;
    int io_threads;
} tmq_broker_t;

int tmq_broker_init(tmq_broker_t* broker, const char* cfg);
void tmq_broker_run(tmq_broker_t* broker);

#endif //TINYMQTT_MQTT_BROKER_H
