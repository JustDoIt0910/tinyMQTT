//
// Created by just do it on 2024/1/23.
//
#include "mqtt_discovery.h"
#include "base/mqtt_util.h"
#include <pthread.h>
#include <strings.h>
#include <uuid/uuid.h>
#include <string.h>

void tmq_redis_lock_init(tmq_redis_lock_t* lock, const char* redis_addr, uint16_t redis_port,
                         const char* lock_name, int lock_ttl)
{
    bzero(lock, sizeof(tmq_redis_lock_t));
    lock->context = redisConnect(redis_addr, redis_port);
    if(lock->context->err)
        fatal_error("connect to redis %s:%hd failed: %s", redis_addr, redis_port, lock->context->errstr);
    lock->lock_name = tmq_str_new(lock_name);
    lock->lock_ttl = lock_ttl;
    lock->node_id = get_uuid();
}

int tmq_redis_lock_acquire(tmq_redis_lock_t* lock)
{
    while(1)
    {
        redisReply* reply = (redisReply*)redisCommand(lock->context, "SET %s %s NX EX %d",
                                                      lock->lock_name, lock->node_id, lock->lock_ttl);
        if(reply!=NULL)
        {
            if(reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0)
            {
                freeReplyObject(reply);
                break;
            }
            freeReplyObject(reply);
            usleep(TRY_LOCK_INTERVAL_MS * 1000);
        }
        else return -1;
    }
    return 0;
}

int tmq_redis_lock_release(tmq_redis_lock_t* lock)
{
    const char* release_script = "if redis.call('get',KEYS[1]) == ARGV[1] then "
                                 "  return redis.call('del',KEYS[1]) "
                                 "else "
                                 "  return 0 "
                                 "end";
    redisReply* reply = redisCommand(lock->context, "eval %s 1 %s %s", release_script, lock->lock_name, lock->node_id);
    if(reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1)
    {
        freeReplyObject(reply);
        return 0;
    }
    if(reply) freeReplyObject(reply);
    return -1;
}

void tmq_redis_lock_destroy(tmq_redis_lock_t* lock)
{
    redisFree(lock->context);
    tmq_str_free(lock->lock_name);
    tmq_str_free(lock->node_id);
}

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
    discovery->ttl = DEFAULT_REGISTRY_TTL;
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