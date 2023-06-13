//
// Created by zr on 23-4-20.
//
#include "mqtt_session.h"
#include "mqtt_io_group.h"
#include "net/mqtt_tcp_conn.h"
#include "base/mqtt_util.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern void mqtt_subscribe_unsubscribe_request(tmq_broker_t* broker, subscribe_unsubscribe_req* sub_unsub_req,
                                               message_ctl_op op);

sending_packet* send_packet_new(tmq_packet_type type, void* pkt, uint16_t packet_id)
{
    sending_packet* sending_pkt = malloc(sizeof(sending_packet));
    if(!sending_pkt) fatal_error("malloc() error: out of memory");
    sending_pkt->packet_id = packet_id;
    sending_pkt->packet.packet_type = type;
    sending_pkt->packet.packet = pkt;
    sending_pkt->send_time = time_now();
    sending_pkt->next = NULL;
    return sending_pkt;
}

tmq_session_t* tmq_session_new(void* upstream, new_message_cb on_new_message,
                               tmq_tcp_conn_t* conn, tmq_str_t client_id,
                               uint8_t clean_session, uint16_t ka, uint8_t max_inflight)
{
    tmq_session_t* session = malloc(sizeof(tmq_session_t));
    if(!session) fatal_error("malloc() error: out of memory");
    bzero(session, sizeof(tmq_session_t));
    session->upstream = upstream;
    session->on_new_message = on_new_message;
    session->state = OPEN;
    session->clean_session = clean_session;
    session->conn = get_ref(conn);
    session->client_id = tmq_str_new(client_id);
    session->keep_alive = ka;
    session->last_pkt_ts = time_now();
    session->inflight_window_size = max_inflight;
    session->inflight_packets = 0;

    unsigned int rand_seed = time(NULL);
    session->next_packet_id = rand_r(&rand_seed) % UINT16_MAX;

    session->sending_queue_head = session->sending_queue_tail = NULL;

    tmq_map_str_init(&session->subscriptions, uint8_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    return session;
}

void tmq_session_handle_subscribe(tmq_session_t* session, tmq_subscribe_pkt* subscribe_pkt)
{
    session->last_pkt_ts = time_now();
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
    session->last_pkt_ts = time_now();
    for(tmq_str_t* tf = tmq_vec_begin(unsubscribe_pkt->topics); tf != tmq_vec_end(unsubscribe_pkt->topics); tf++)
        tmq_map_erase(session->subscriptions, *tf);
    subscribe_unsubscribe_req req = {
            .client_id = tmq_str_new(session->client_id),
            .sub_unsub_pkt.unsubscribe_pkt = *unsubscribe_pkt
    };
    mqtt_subscribe_unsubscribe_request((tmq_broker_t*)session->upstream, &req, UNSUBSCRIBE);
}

void tmq_session_handle_publish(tmq_session_t* session, tmq_publish_pkt* publish_pkt)
{
    session->last_pkt_ts = time_now();
    /* qos 0 message, deliver to the upstream(broker/client) directly. */
    if(PUBLISH_QOS(publish_pkt->flags) == 0)
    {
        tmq_message message = {
                .qos = 0,
                .message = tmq_str_new(publish_pkt->payload)
        };
        session->on_new_message(session->upstream, publish_pkt->topic, &message, PUBLISH_RETAIN(publish_pkt->flags));
        tmq_publish_pkt_cleanup(publish_pkt);
    }
}

void tmq_session_handle_pingreq(tmq_session_t* session)
{
    session->last_pkt_ts = time_now();
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

void tmq_session_publish(tmq_session_t* session, char* topic, char* payload, uint8_t qos, uint8_t retain)
{
    tmq_publish_pkt* publish_pkt = malloc(sizeof(tmq_publish_pkt));
    bzero(publish_pkt, sizeof(tmq_publish_pkt));
    publish_pkt->topic = tmq_str_new(topic);
    publish_pkt->payload = tmq_str_new(payload);
    publish_pkt->flags |= retain;

    tmq_any_packet_t pkt = {
            .packet_type = MQTT_PUBLISH,
            .packet = publish_pkt
    };
    int send_now = 1;
    /* if qos = 0, fire and forget */
    if(qos >= 0)
    {
        publish_pkt->packet_id = session->next_packet_id;
        session->next_packet_id = session->next_packet_id == UINT16_MAX ? 0 : session->next_packet_id++;
        publish_pkt->flags |= (qos << 1);

        sending_packet* sending_pkt = send_packet_new(MQTT_PUBLISH, tmq_publish_pkt_clone(publish_pkt),
                                                      publish_pkt->packet_id);
        /* if the sending queue is empty */
        if(!session->sending_queue_tail)
            session->sending_queue_head = session->sending_queue_tail = sending_pkt;
        else
        {
            session->sending_queue_tail->next = sending_pkt;
            session->sending_queue_tail = sending_pkt;
        }
        /* if the number of inflight packets less than the inflight window size,
         * this packet can be sent immediately */
        if(session->inflight_packets < session->inflight_window_size)
            session->inflight_packets++;
        /* otherwise, it must wait for sending in the sending_queue */
        else
        {
            
        }
    }
    if(send_now) tmq_session_send_packet(session, &pkt);
}