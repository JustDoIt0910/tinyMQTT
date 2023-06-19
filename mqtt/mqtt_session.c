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
extern void on_mqtt_subscribe_response(tiny_mqtt* mqtt, tmq_suback_pkt * suback_pkt);
extern void on_mqtt_unsubscribe_response(tiny_mqtt* mqtt, tmq_unsuback_pkt* unsuback_pkt);

static sending_packet* sending_packet_new(tmq_packet_type type, void* pkt, uint16_t packet_id)
{
    sending_packet* sending_pkt = malloc(sizeof(sending_packet));
    if(!sending_pkt) fatal_error("malloc() error: out of memory");
    sending_pkt->packet_id = packet_id;
    sending_pkt->packet.packet_type = type;
    sending_pkt->packet.packet = pkt;
    sending_pkt->next = NULL;
    sending_pkt->send_time = 0;
    return sending_pkt;
}

static int accknowledge(tmq_session_t* session, uint16_t packet_id, tmq_packet_type type, int qos)
{
    int ack_success = 0;
    pthread_mutex_lock(&session->sending_queue_lk);
    sending_packet** p = &session->sending_queue_head;
    int cnt = 0;
    while(*p && cnt++ < session->inflight_packets)
    {
        if((*p)->packet_id != packet_id || (*p)->packet.packet_type != type)
        {
            p = &((*p)->next);
            continue;
        }
        if(type == MQTT_PUBLISH)
        {
            tmq_publish_pkt* pkt = (*p)->packet.packet;
            if(PUBLISH_QOS(pkt->flags) != qos)
            {
                p = &((*p)->next);
                continue;
            }
            if(session->on_publish_finish)
                session->on_publish_finish(session->upstream);
        }
        sending_packet* next = (*p)->next;
        sending_packet* remove = *p;
        *p = next;
        if(session->sending_queue_tail == remove)
            session->sending_queue_tail = session->sending_queue_head ? (sending_packet*) p: NULL;
        tmq_any_pkt_cleanup(&remove->packet);
        free(remove);
        session->inflight_packets--;
        ack_success = 1;
        break;
    }
    if(session->pending_pointer && ack_success)
    {
        tmq_any_packet_t send = session->pending_pointer->packet;
        if(send.packet_type == MQTT_PUBLISH)
            send.packet = tmq_publish_pkt_clone(session->pending_pointer->packet.packet);
        else send.packet = tmq_pubrel_pkt_clone(session->pending_pointer->packet.packet);
        tmq_session_send_packet(session, &send);

        session->pending_pointer->send_time = time_now();
        session->pending_pointer = session->pending_pointer->next;
        session->inflight_packets++;
    }
    pthread_mutex_unlock(&session->sending_queue_lk);
    return ack_success;
}

static int store_sending_packet(tmq_session_t* session, sending_packet* sending_pkt)
{
    pthread_mutex_lock(&session->sending_queue_lk);
    int send_now = 1;
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
        send_now = 0;
        if(!session->pending_pointer)
            session->pending_pointer = sending_pkt;
    }
    pthread_mutex_unlock(&session->sending_queue_lk);
    return send_now;
}

static void resend_messages(void* arg)
{
    int64_t now = time_now();
    tmq_session_t* session = arg;
    pthread_mutex_lock(&session->sending_queue_lk);
    sending_packet* sending_pkt = session->sending_queue_head;
    int cnt = 0;
    while(sending_pkt && cnt++ < session->inflight_packets)
    {
        if(now - sending_pkt->send_time >= SEC_US(RESEND_INTERVAL))
        {
            tmq_any_packet_t send = sending_pkt->packet;
            if(send.packet_type == MQTT_PUBLISH)
                send.packet = tmq_publish_pkt_clone(sending_pkt->packet.packet);
            else send.packet = tmq_pubrel_pkt_clone(sending_pkt->packet.packet);
            tmq_session_send_packet(session, &send);
        }
        sending_pkt = sending_pkt->next;
    }
    pthread_mutex_unlock(&session->sending_queue_lk);
}

tmq_session_t* tmq_session_new(void* upstream, new_message_cb on_new_message, close_cb on_close, tmq_tcp_conn_t* conn,
                               char* client_id, uint8_t clean_session, uint16_t keep_alive, char* will_topic,
                               char* will_message, uint8_t will_qos, uint8_t will_retain, uint8_t max_inflight)
{
    tmq_session_t* session = malloc(sizeof(tmq_session_t));
    if(!session) fatal_error("malloc() error: out of memory");
    bzero(session, sizeof(tmq_session_t));
    session->upstream = upstream;
    session->on_new_message = on_new_message;
    session->on_close = on_close;
    session->state = OPEN;
    session->clean_session = clean_session;
    session->conn = get_ref(conn);
    session->client_id = tmq_str_new(client_id);
    session->keep_alive = keep_alive;
    session->last_pkt_ts = time_now();
    session->inflight_window_size = max_inflight;
    session->inflight_packets = 0;
    if(will_topic)
    {
        session->will_publish_req.topic = tmq_str_new(will_topic);
        session->will_publish_req.message.message = tmq_str_new(will_message);
        session->will_publish_req.message.qos = will_qos;
        session->will_publish_req.retain = will_retain;
    }
    unsigned int rand_seed = time(NULL);
    session->next_packet_id = rand_r(&rand_seed) % UINT16_MAX;

    session->sending_queue_head = session->sending_queue_tail = NULL;

    tmq_map_str_init(&session->subscriptions, uint8_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_map_32_init(&session->qos2_packet_ids, uint8_t, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    pthread_mutex_init(&session->lk, NULL);
    pthread_mutex_init(&session->sending_queue_lk, NULL);
    return session;
}

void tmq_session_start(tmq_session_t* session)
{
    resend_messages(session);
    if (session->inflight_packets > 0)
    {
        tmq_timer_t* timer = tmq_timer_new(SEC_MS(RESEND_INTERVAL), 1, resend_messages, session);
        session->resend_timer = tmq_event_loop_add_timer(session->conn->loop, timer);
    }
}

void tmq_session_close(tmq_session_t* session)
{
    if(session->state == OPEN)
    {
        tmq_event_loop_cancel_timer(session->conn->loop, session->resend_timer);
        session->state = CLOSED;
    }
    if(session->conn)
        release_ref(session->conn);
    if(session->on_close)
        session->on_close(session->upstream, session);
}

void tmq_session_free(tmq_session_t* session)
{
    tmq_str_free(session->client_id);
    tmq_map_free(session->subscriptions);
    tmq_map_free(session->qos2_packet_ids);
    sending_packet* sending_pkt = session->sending_queue_head;
    while(sending_pkt)
    {
        sending_packet * next = sending_pkt->next;
        tmq_any_pkt_cleanup(&sending_pkt->packet);
        free(sending_pkt);
        sending_pkt = next;
    }
    pthread_mutex_destroy(&session->sending_queue_lk);
    pthread_mutex_destroy(&session->lk);
    free(session);
}

void tmq_session_resume(tmq_session_t* session, tmq_tcp_conn_t* conn, uint16_t keep_alive, char* will_topic,
                        char* will_message, uint8_t will_qos, uint8_t will_retain)
{
    session->conn = get_ref(conn);
    session->state = OPEN;
    session->last_pkt_ts = time_now();
    session->keep_alive = keep_alive;
    session->will_publish_req.topic = NULL;
    session->will_publish_req.message.message = NULL;
    if(will_topic)
    {
        session->will_publish_req.topic = tmq_str_new(will_topic);
        session->will_publish_req.message.message = tmq_str_new(will_message);
        session->will_publish_req.message.qos = will_qos;
        session->will_publish_req.retain = will_retain;
    }
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
    mqtt_subscribe_unsubscribe_request((tmq_broker_t*) session->upstream, &req, SUBSCRIBE);
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
    mqtt_subscribe_unsubscribe_request((tmq_broker_t*) session->upstream, &req, UNSUBSCRIBE);
}

void tmq_session_handle_suback(tmq_session_t* session, tmq_suback_pkt* suback_pkt)
{
    session->last_pkt_ts = time_now();
    on_mqtt_subscribe_response((tiny_mqtt*) session->upstream, suback_pkt);
}

void tmq_session_handle_unsuback(tmq_session_t* session, tmq_unsuback_pkt* unsuback_pkt)
{
    session->last_pkt_ts = time_now();
    on_mqtt_unsubscribe_response((tiny_mqtt*) session->upstream, unsuback_pkt);
}

void tmq_session_handle_publish(tmq_session_t* session, tmq_publish_pkt* publish_pkt)
{
    session->last_pkt_ts = time_now();
    tmq_message message = {
            .qos = PUBLISH_QOS(publish_pkt->flags),
            .message = tmq_str_new(publish_pkt->payload)
    };
    /* for qos2 message, check if it is a redelivery */
    if(PUBLISH_QOS(publish_pkt->flags) == 2)
    {
        /* if this is the first time that receive this publish message,
         * store the packet id and deliver this message */
        if(tmq_map_get(session->qos2_packet_ids, publish_pkt->packet_id) == NULL)
            tmq_map_put(session->qos2_packet_ids, publish_pkt->packet_id, 1);
            /* if it is a redelivered message, just discard it. */
        else
        {
            tmq_publish_pkt_cleanup(publish_pkt);
            return;
        }
    }
    session->on_new_message(session->upstream, publish_pkt->topic, &message, PUBLISH_RETAIN(publish_pkt->flags));
    tmq_publish_pkt_cleanup(publish_pkt);
}

void tmq_session_handle_pingreq(tmq_session_t* session) {session->last_pkt_ts = time_now();}

void tmq_session_handle_pingresp(tmq_session_t* session) {session->last_pkt_ts = time_now();}

void tmq_session_handle_puback(tmq_session_t* session, tmq_puback_pkt* puback_pkt)
{
    session->last_pkt_ts = time_now();
    pthread_mutex_lock(&session->lk);

    accknowledge(session, puback_pkt->packet_id, MQTT_PUBLISH, 1);
    if(session->inflight_packets == 0)
        tmq_event_loop_cancel_timer(session->conn->loop, session->resend_timer);

    pthread_mutex_unlock(&session->lk);
}

void tmq_session_handle_pubrec(tmq_session_t* session, tmq_pubrec_pkt* pubrec_pkt)
{
    session->last_pkt_ts = time_now();
    if(accknowledge(session, pubrec_pkt->packet_id, MQTT_PUBLISH, 2))
    {
        tmq_pubrel_pkt* pubrel_pkt = malloc(sizeof(tmq_pubrel_pkt));
        pubrel_pkt->packet_id = pubrec_pkt->packet_id;

        sending_packet* sending_pkt = sending_packet_new(MQTT_PUBREL, pubrel_pkt, pubrel_pkt->packet_id);
        if(store_sending_packet(session, sending_pkt))
        {
            tmq_any_packet_t pkt = {
                    .packet_type = MQTT_PUBREL,
                    .packet = tmq_pubrel_pkt_clone(pubrel_pkt)
            };
            sending_pkt->send_time = time_now();
            tmq_session_send_packet(session, &pkt);
        }
    }
}

void tmq_session_handle_pubrel(tmq_session_t* session, tmq_pubrel_pkt* pubrel_pkt)
{
    session->last_pkt_ts = time_now();
    tmq_map_erase(session->qos2_packet_ids, pubrel_pkt->packet_id);
}

void tmq_session_handle_pubcomp(tmq_session_t* session, tmq_pubcomp_pkt* pubcomp_pkt)
{
    session->last_pkt_ts = time_now();
    pthread_mutex_lock(&session->lk);

    accknowledge(session, pubcomp_pkt->packet_id, MQTT_PUBREL, -1);
    if(session->inflight_packets == 0)
        tmq_event_loop_cancel_timer(session->conn->loop, session->resend_timer);

    pthread_mutex_unlock(&session->lk);
}

void tmq_session_send_packet(tmq_session_t* session, tmq_any_packet_t* pkt)
{
    tmq_io_group_t* group = session->conn->group;
    /* if the underlying tcp connection doesn't belong to an io-group,
     * send the packet directly in this thread */
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

void tmq_session_publish(tmq_session_t* session, const char* topic, const char* payload, uint8_t qos, uint8_t retain)
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
    if(qos > 0)
    {
        publish_pkt->packet_id = session->next_packet_id;
        session->next_packet_id = session->next_packet_id == UINT16_MAX ? 0 : session->next_packet_id + 1;
        publish_pkt->flags |= (qos << 1);

        pthread_mutex_lock(&session->lk);

        int start_resend = session->inflight_packets == 0;
        tmq_publish_pkt* stored_pkt = tmq_publish_pkt_clone(publish_pkt);

        sending_packet* sending_pkt = sending_packet_new(MQTT_PUBLISH, stored_pkt, publish_pkt->packet_id);
        send_now = store_sending_packet(session, sending_pkt);
        if(send_now) sending_pkt->send_time = time_now();

        if(start_resend && tmq_event_loop_resume_timer(session->conn->loop, session->resend_timer) < 0)
        {
            tmq_timer_t* timer = tmq_timer_new(SEC_MS(RESEND_INTERVAL), 1, resend_messages, session);
            session->resend_timer = tmq_event_loop_add_timer(session->conn->loop, timer);
        }
        pthread_mutex_unlock(&session->lk);
    }
    if(send_now)
        tmq_session_send_packet(session, &pkt);
    else tmq_publish_pkt_cleanup(publish_pkt);
}

void tmq_session_store_publish(tmq_session_t* session, const char* topic, const char* payload, uint8_t qos, uint8_t retain)
{
    if(qos == 0) return;
    tmq_publish_pkt* publish_pkt = malloc(sizeof(tmq_publish_pkt));
    bzero(publish_pkt, sizeof(tmq_publish_pkt));
    publish_pkt->topic = tmq_str_new(topic);
    publish_pkt->payload = tmq_str_new(payload);
    publish_pkt->flags |= retain;
    publish_pkt->flags |= (qos << 1);
    publish_pkt->packet_id = session->next_packet_id;
    session->next_packet_id = session->next_packet_id == UINT16_MAX ? 0 : session->next_packet_id + 1;

    sending_packet* sending_pkt = sending_packet_new(MQTT_PUBLISH, publish_pkt, publish_pkt->packet_id);
    store_sending_packet(session, sending_pkt);
}

void tmq_session_subscribe(tmq_session_t* session, const char* topic_filter, uint8_t qos)
{
    topic_list topics = tmq_vec_make(topic_filter_qos);
    topic_filter_qos topic = {
            .topic_filter = tmq_str_new(topic_filter),
            .qos = qos
    };
    tmq_vec_push_back(topics, topic);

    tmq_subscribe_pkt* subscribe_pkt = malloc(sizeof(tmq_subscribe_pkt));
    subscribe_pkt->packet_id = session->next_packet_id;
    subscribe_pkt->topics = topics;
    session->next_packet_id = session->next_packet_id == UINT16_MAX ? 0 : session->next_packet_id + 1;
    tmq_any_packet_t pkt = {
            .packet_type = MQTT_SUBSCRIBE,
            .packet = subscribe_pkt
    };
    tmq_session_send_packet(session, &pkt);
}

void tmq_session_unsubscribe(tmq_session_t* session, const char* topic_filter)
{
    str_vec topics = tmq_vec_make(tmq_str_t);
    tmq_vec_push_back(topics, tmq_str_new(topic_filter));

    tmq_unsubscribe_pkt* unsubscribe_pkt = malloc(sizeof(tmq_unsubscribe_pkt));
    unsubscribe_pkt->packet_id =  session->next_packet_id;
    unsubscribe_pkt->topics = topics;
    session->next_packet_id = session->next_packet_id == UINT16_MAX ? 0 : session->next_packet_id + 1;
    tmq_any_packet_t pkt = {
            .packet_type = MQTT_UNSUBSCRIBE,
            .packet = unsubscribe_pkt
    };
    tmq_session_send_packet(session, &pkt);
}

void tmq_session_set_publish_finish_callback(tmq_session_t* session, publish_finish_cb cb)
{
    if(!session) return;
    session->on_publish_finish = cb;
}