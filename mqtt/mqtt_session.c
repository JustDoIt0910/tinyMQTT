//
// Created by zr on 23-4-20.
//
#include "mqtt_session.h"
#include "mqtt_io_group.h"
#include "mqtt_types.h"
#include "net/mqtt_tcp_conn.h"
#include "base/mqtt_util.h"
#include <stdlib.h>
#include <string.h>

extern void mqtt_subscribe_unsubscribe_request(tmq_broker_t* broker, subscribe_unsubscribe_req* sub_unsub_req,
                                               message_ctl_op op);

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

void tmq_session_handle_subscribe(tmq_session_t* session, tmq_subscribe_pkt* subscribe_pkt)
{
    topic_filter_qos* tf = tmq_vec_begin(subscribe_pkt->topics);
    for(; tf != tmq_vec_end(subscribe_pkt->topics); tf++)
        tmq_map_put(session->subscriptions, tf->topic_filter, tf->qos);
    subscribe_unsubscribe_req req = {
            .client_id = tmq_str_new(session->client_id),
            .sub_unsub_pkt.subscribe_pkt = *subscribe_pkt
    };
    mqtt_subscribe_unsubscribe_request((tmq_broker_t*)session->upstream, &req, SUBSCRIBE);
}

void tmq_session_handle_unsubscribe(tmq_session_t* session, tmq_unsubscribe_pkt* unsubscribe_pkt)
{
    for(tmq_str_t* tf = tmq_vec_begin(unsubscribe_pkt->topics); tf != tmq_vec_end(unsubscribe_pkt->topics); tf++)
        tmq_map_erase(session->subscriptions, *tf);
    subscribe_unsubscribe_req req = {
            .client_id = tmq_str_new(session->client_id),
            .sub_unsub_pkt.unsubscribe_pkt = *unsubscribe_pkt
    };
    mqtt_subscribe_unsubscribe_request((tmq_broker_t*)session->upstream, &req, UNSUBSCRIBE);
}

void tmq_session_send_packet(tmq_session_t* session, tmq_any_packet_t* pkt)
{
    tmq_io_group_t* group = session->conn->group;
    /* if the underlying conn doesn't belong to an io-group, send the packet directly */
    if(!group)
    {
        send_any_packet(session->conn, pkt);
        tmq_any_pkt_cleanup(pkt);
    }
    /* otherwise, send the packet in the io-group which the connection belongs to */
    else
    {
        packet_send_req req = {
                .conn = get_ref(session->conn),
                .pkt = *pkt
        };
        pthread_mutex_lock(&group->sending_packets_lk);
        tmq_vec_push_back(group->sending_packets, req);
        pthread_mutex_unlock(&group->sending_packets_lk);

        tmq_notifier_notify(&group->sending_packets_notifier);
    }
}