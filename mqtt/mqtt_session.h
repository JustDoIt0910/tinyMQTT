//
// Created by zr on 23-4-20.
//

#ifndef TINYMQTT_MQTT_SESSION_H
#define TINYMQTT_MQTT_SESSION_H
#include "net/mqtt_tcp_conn.h"
#include "mqtt/mqtt_types.h"
#include "store/mqtt_msg_store.h"

typedef struct tmq_session_s tmq_session_t;
typedef enum session_state_e{OPEN, CLOSED} session_state_e;

typedef void(*new_message_cb)(void* arg, tmq_session_t* session, char* topic, mqtt_message* message,
                              uint8_t retain, char* username, char* client_id, int is_tunneled_pub);
typedef void(*publish_finish_cb)(void* upstream, uint16_t packet_id, uint8_t qos);
typedef void(*close_cb)(void* upstream, tmq_session_t* session, int force_clean);

typedef tmq_map(char*, uint8_t) subscription_map;
typedef tmq_map(uint32_t, uint8_t)  packet_id_set;

typedef struct tmq_session_s
{
    REF_COUNTED_MEMBERS
    new_message_cb on_new_message;
    publish_finish_cb on_publish_finish;
    close_cb on_close;

    tmq_str_t client_id;
    tmq_str_t username;
    tmq_str_t will_message;
    tmq_str_t will_topic;
    tmq_tcp_conn_t* conn;
    void* upstream;
    message_store_t* message_store;
    uint64_t store_timestamp;
    int64_t last_pkt_ts;
    uint16_t next_packet_id;
    uint16_t keep_alive;
    session_state_e state;
    uint8_t clean_session;
    uint8_t inflight_window_size;
    uint8_t inflight_packets;
    uint8_t will_qos;
    uint8_t will_retain;
    uint8_t io_context_idx;

    subscription_map subscriptions;
    packet_id_set qos2_packet_ids;
} tmq_session_t;

#define SESSION_SHARE(session) ((tmq_session_t*) get_ref((tmq_ref_counted_t*) (session)))
#define SESSION_RELEASE(session) release_ref((tmq_ref_counted_t*) (session))

tmq_session_t* tmq_session_new(void* upstream, new_message_cb on_new_message, close_cb on_close, tmq_tcp_conn_t* conn,
                               char* client_id, char* username, uint8_t clean_session, uint16_t keep_alive,
                               char* will_topic, char* will_message, uint8_t will_qos, uint8_t will_retain,
                               uint8_t max_inflight, message_store_t* message_store);
void tmq_session_close(tmq_session_t* session, int force_clean);
void tmq_session_publish(tmq_session_t* session, tmq_str_t topic, tmq_str_t payload, uint8_t qos,
                         uint8_t retain, int store_only);
void tmq_session_subscribe(tmq_session_t* session, const char* topic_filter, uint8_t qos);
void tmq_session_unsubscribe(tmq_session_t* session, const char* topic_filter);
void tmq_session_send_packet(tmq_session_t* session, tmq_any_packet_t* pkt, int queue);
void tmq_session_start(tmq_session_t* session);
void tmq_session_resume(tmq_session_t* session, tmq_tcp_conn_t* conn, tmq_str_t username, uint16_t keep_alive, char* will_topic,
                        char* will_message, uint8_t will_qos, uint8_t will_retain);
void tmq_session_set_publish_finish_callback(tmq_session_t* session, publish_finish_cb cb);
void tmq_session_free(tmq_session_t* session);

#endif //TINYMQTT_MQTT_SESSION_H
