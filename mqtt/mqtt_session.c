//
// Created by zr on 23-4-20.
//
#include "mqtt_session.h"
#include "mqtt_types.h"
#include "net/mqtt_tcp_conn.h"
#include "base/mqtt_util.h"
#include <stdlib.h>
#include <string.h>

extern void mqtt_subscribe_request(tmq_broker_t* broker, subscribe_req* sub_req);

tmq_session_t* tmq_session_new(void* upstream, tmq_tcp_conn_t* conn, tmq_str_t client_id, int clean_session)
{
    tmq_session_t* session = malloc(sizeof(tmq_session_t));
    if(!session) fatal_error("malloc() error: out of memory");
    bzero(session, sizeof(tmq_session_t));
    session->upstream = upstream;
    session->state = OPEN;
    session->clean_session = clean_session;
    session->conn = get_ref(conn);
    session->client_id = tmq_str_new(client_id);
    tmq_map_str_init(&session->subscriptions, uint8_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    return session;
}

void tmq_session_handle_subscribe(tmq_session_t* session, tmq_subscribe_pkt subscribe_pkt)
{
    topic_filter_qos* tf = tmq_vec_begin(subscribe_pkt.topics);
    for(; tf != tmq_vec_end(subscribe_pkt.topics); tf++)
        tmq_map_put(session->subscriptions, tf->topic_filter, tf->qos);
    subscribe_req req = {
            .client_id = tmq_str_new(session->client_id),
            .topic_filters = tmq_vec_make(topic_filter_qos)
    };
    tmq_vec_swap(req.topic_filters, subscribe_pkt.topics);
    tmq_subscribe_pkt_cleanup(&subscribe_pkt);
    mqtt_subscribe_request((tmq_broker_t*)session->upstream, &req);
}