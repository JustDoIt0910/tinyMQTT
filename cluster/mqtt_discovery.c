//
// Created by just do it on 2024/1/23.
//
#include "mqtt_discovery.h"
#include "base/mqtt_util.h"
#include <pthread.h>
#include <strings.h>

void tmq_redis_discovery_init(tmq_event_loop_t* loop, tmq_redis_discovery_t* discovery, const char* addr, uint16_t port,
                              new_node_cb_f on_new_node, remove_node_cb_f on_remove_node)
{
    bzero(discovery, sizeof(tmq_redis_discovery_t));
    discovery->sub_context = redisConnect(addr, port);
    if(discovery->sub_context->err)
        fatal_error("connect to redis %s:%hd failed: %s", addr, port, discovery->sub_context->errstr);
    discovery->pub_context = redisConnect(addr, port);
    if(discovery->pub_context->err)
        fatal_error("connect to redis %s:%hd failed: %s", addr, port, discovery->pub_context->errstr);
    discovery->ttl = DEFAULT_TTL;
    discovery->loop = loop;
    discovery->on_new_node = on_new_node;
    discovery->on_remove_node = on_remove_node;
}

static void discovery_keep_alive(void* arg)
{
    tmq_redis_discovery_t* discovery = arg;
    redisReply* reply = redisCommand(discovery->pub_context, "EXPIRE %s %d", discovery->node_key, discovery->ttl);
    if(reply!=NULL) freeReplyObject(reply);
}

void tmq_redis_discovery_register(tmq_redis_discovery_t* discovery, const char* ip, uint16_t port)
{
    sprintf(discovery->node_key, "nodes:%s:%hu", ip, port);
    redisReply* reply = (redisReply*)redisCommand(discovery->pub_context, "SETEX %s %d on", discovery->node_key, discovery->ttl);
    if(reply!=NULL) freeReplyObject(reply);
    tmq_timer_t* timer = tmq_timer_new(1000, 1, discovery_keep_alive, discovery);
    tmq_event_loop_add_timer(discovery->loop, timer);
}

static void* discovery_listen_routine(void* arg)
{
    tmq_redis_discovery_t* discovery = arg;
    while(1)
    {
        void *_reply = NULL;
        if (redisGetReply(discovery->sub_context, &_reply) != REDIS_OK)
            continue;
        redisReply* reply = (redisReply*)_reply;
        str_vec strs = tmq_str_split(reply->element[2]->str, ":");
        uint16_t port = atoi(*tmq_vec_at(strs, 3));
        if(tmq_str_equal(reply->element[3]->str, "set"))
        {
            if(discovery->on_new_node)
                discovery->on_new_node(discovery->ctx, *tmq_vec_at(strs, 2), port);
        }
        else if(tmq_str_equal(reply->element[3]->str, "expired"))
        {
            if(discovery->on_remove_node)
                discovery->on_remove_node(discovery->ctx, *tmq_vec_at(strs, 2), port);
        }
        freeReplyObject(reply);
        for(tmq_str_t* s = tmq_vec_begin(strs); s != tmq_vec_end(strs); s++)
            tmq_str_free(*s);
        tmq_vec_free(strs);
    }
}

void tmq_redis_discovery_listen(tmq_redis_discovery_t* discovery)
{
    redisReply* reply = (redisReply*)redisCommand(discovery->sub_context, "PSUBSCRIBE __keyspace@0__:nodes:*");
    if(!reply || reply->type != REDIS_REPLY_ARRAY)
    {
        freeReplyObject(reply);
        tlog_error("redis subscribe error");
        return;
    }
    pthread_t listen_thread;
    pthread_create(&listen_thread, NULL, discovery_listen_routine, discovery);
    pthread_detach(listen_thread);
}

void tmq_redis_discovery_set_context(tmq_redis_discovery_t* discovery, void* ctx) { discovery->ctx = ctx; }