//
// Created by just do it on 2024/1/23.
//

#ifndef TINYMQTT_MQTT_DISCOVERY_H
#define TINYMQTT_MQTT_DISCOVERY_H
#include <hiredis/hiredis.h>
#include "event/mqtt_event.h"
#include "base/mqtt_str.h"

#define DEFAULT_TTL 10

typedef void(*new_node_cb_f)(void* arg, const char* ip, uint16_t port);
typedef void(*remove_node_cb_f)(void* arg, const char* ip, uint16_t port);
typedef struct tmq_redis_discovery_s
{
    tmq_event_loop_t* loop;
    char node_key[100];
    redisContext* sub_context;
    redisContext* pub_context;
    new_node_cb_f on_new_node;
    remove_node_cb_f on_remove_node;
    int ttl;
    void* ctx;
} tmq_redis_discovery_t;

void tmq_redis_discovery_init(tmq_event_loop_t* loop, tmq_redis_discovery_t* discovery, const char* addr, uint16_t port,
                              new_node_cb_f on_new_node, remove_node_cb_f on_remove_node);
void tmq_redis_discovery_register(tmq_redis_discovery_t* discovery, const char* ip, uint16_t port);
void tmq_redis_discovery_listen(tmq_redis_discovery_t* discovery);
void tmq_redis_discovery_set_context(tmq_redis_discovery_t* discovery, void* ctx);

#endif //TINYMQTT_MQTT_DISCOVERY_H
