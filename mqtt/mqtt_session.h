//
// Created by zr on 23-4-20.
//

#ifndef TINYMQTT_MQTT_SESSION_H
#define TINYMQTT_MQTT_SESSION_H
#include "net/mqtt_tcp_conn.h"
#include "mqtt/mqtt_types.h"

#define RESEND_INTERVAL 1

typedef struct sending_packet
{
    struct sending_packet* next;
    int64_t send_time;
    uint16_t packet_id;
    tmq_any_packet_t packet;
} sending_packet;

typedef struct tmq_session_s tmq_session_t;
typedef enum session_state_e{OPEN, CLOSED} session_state_e;

typedef void(*new_message_cb)(void* upstream, char* topic, tmq_message* message, uint8_t retain);
typedef void(*close_cb)(void* upstream, tmq_session_t* session);

typedef tmq_map(char*, uint8_t) subscription_map;
typedef tmq_map(uint32_t, uint8_t)  packet_id_set;

typedef struct tmq_session_s
{
    tmq_str_t client_id;
    tmq_tcp_conn_t* conn;
    session_state_e state;
    uint8_t clean_session;
    subscription_map subscriptions;

    uint16_t keep_alive;
    int64_t last_pkt_ts;
    uint16_t next_packet_id;
    uint8_t inflight_window_size;
    uint8_t inflight_packets;
    tmq_timerid_t resend_timer;
    pthread_mutex_t lk;

    void* upstream;
    new_message_cb on_new_message;
    close_cb on_close;

    pthread_mutex_t sending_queue_lk;
    sending_packet* sending_queue_head, *sending_queue_tail;
    sending_packet* pending_pointer;

    packet_id_set qos2_packet_ids;
    publish_req will_publish_req;
} tmq_session_t;

tmq_session_t* tmq_session_new(void* upstream, new_message_cb on_new_message, close_cb on_close, tmq_tcp_conn_t* conn,
                               char* client_id, uint8_t clean_session, uint16_t keep_alive, char* will_topic,
                               char* will_message, uint8_t will_qos, uint8_t will_retain, uint8_t max_inflight);
void tmq_session_close(tmq_session_t* session);
void tmq_session_free(tmq_session_t* session);
void tmq_session_publish(tmq_session_t* session, const char* topic, const char* payload, uint8_t qos, uint8_t retain);
void tmq_session_store_publish(tmq_session_t* session, const char* topic, const char* payload,
                               uint8_t qos, uint8_t retain);
void tmq_session_subscribe(tmq_session_t* session, const char* topic_filter, uint8_t qos);
void tmq_session_send_packet(tmq_session_t* session, tmq_any_packet_t* pkt);
void tmq_session_start(tmq_session_t* session);
void tmq_session_resume(tmq_session_t* session, tmq_tcp_conn_t* conn, uint16_t keep_alive, char* will_topic,
                        char* will_message, uint8_t will_qos, uint8_t will_retain);

#endif //TINYMQTT_MQTT_SESSION_H
