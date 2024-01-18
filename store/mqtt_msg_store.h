//
// Created by just do it on 2024/1/10.
//

#ifndef TINYMQTT_MQTT_MSG_STORE_H
#define TINYMQTT_MQTT_MSG_STORE_H
#include "mqtt/mqtt_packet.h"

typedef struct sending_packet_s
{
    struct sending_packet_s* next;
    uint64_t store_timestamp;
    int64_t send_time;
    uint16_t packet_id;
    tmq_any_packet_t packet;
} sending_packet_t;

typedef struct tmq_session_s tmq_session_t;
typedef struct message_store_s message_store_t;
typedef sending_packet_t*(*acknowledge_f)(message_store_t* store, tmq_session_t* session, uint16_t packet_id,
        tmq_packet_type type, int qos);
typedef int(*store_message_f)(message_store_t* store, tmq_session_t* session, sending_packet_t* sending_pkt);

#define MESSAGE_STORE_PUBLIC_MEMBERS    \
uint16_t total_size;                    \
sending_packet_t* sending_queue_head;     \
sending_packet_t* sending_queue_tail;     \
sending_packet_t* pending_pointer;        \
acknowledge_f acknowledge_and_next;     \
store_message_f store_message;

typedef struct message_store_s {MESSAGE_STORE_PUBLIC_MEMBERS} message_store_t;

typedef message_store_t message_store_memory_t;

#define HAS_BUFFERED_MESSAGES(states)   ((states) & 0x01)
#define SET_BUFFERED_MESSAGES(states)   ((states) |= 0x01)
#define CLEAR_BUFFERED_MESSAGES(states) ((states) &= ~0x01)

#define HAS_STORED_MESSAGES(states)     ((states) & 0x02)
#define SET_STORED_MESSAGES(states)     ((states) |= 0x02)
#define CLEAR_STORED_MESSAGES(states)   ((states) &= ~0x02)

#define STORING(states)                 ((states) & 0x04)
#define SET_STORING(states)             ((states) |= 0x04)
#define CLEAR_STORING(states)           ((states) &= ~0x04)

#define FETCHING(states)                ((states) & 0x08)
#define SET_FETCHING(states)            ((states) |= 0x08)
#define CLEAR_FETCHING(states)          ((states) &= ~0x08)

#define NEED_STORE(states)              ((states) & 0x10)
#define SET_NEED_STORE(states)          ((states) |= 0x10)
#define CLEAR_NEED_STORE(states)        ((states) &= ~0x10)

#define NEED_FETCH(states)              ((states) & 0x20)
#define SET_NEED_FETCH(states)          ((states) |= 0x20)
#define CLEAR_NEED_FETCH(states)        ((states) &= ~0x20)

typedef struct message_store_mongodb_s
{
    MESSAGE_STORE_PUBLIC_MEMBERS
    sending_packet_t* buffer_queue_head;
    sending_packet_t** buffer_queue_tail;
    uint16_t trigger;
    uint16_t buffer_size;
    uint8_t states;
} message_store_mongodb_t;

message_store_t* tmq_message_store_memory_new();
message_store_t* tmq_message_store_mongodb_new(uint16_t trigger);

#endif //TINYMQTT_MQTT_MSG_STORE_H
