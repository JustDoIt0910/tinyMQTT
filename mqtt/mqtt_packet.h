//
// Created by zr on 23-4-30.
//

#ifndef TINYMQTT_MQTT_PACKET_H
#define TINYMQTT_MQTT_PACKET_H
#include "base/mqtt_str.h"
#include <stdint.h>

typedef enum tmq_packet_type_e
{
    MQTT_CONNECT = 1, MQTT_CONNACK,
    MQTT_PUBLISH, MQTT_PUBACK, MQTT_PUBREC, MQTT_PUBREL, MQTT_PUBCOMP,
    MQTT_SUBSCRIBE, MQTT_SUBACK,
    MQTT_UNSUBSCRIBE, MQTT_UNSUBACK,
    MQTT_PINGREQ, MQTT_PINGRESP,
    MQTT_DISCONNECT
} tmq_packet_type;

typedef struct tmq_packet_t
{
    tmq_packet_type packet_type;
    void* packet;
} tmq_packet_t;

typedef struct tmq_connect_pkt
{
    uint8_t flags;
    uint16_t keep_alive;

    tmq_str_t client_id;
    tmq_str_t will_topic;
    tmq_str_t will_message;
    tmq_str_t username;
    tmq_str_t password;
} tmq_connect_pkt;

#define CONNECT_USERNAME_FLAG(flags)     ((flags) & 0x80)
#define CONNECT_PASSWORD_FLAG(flags)     ((flags) & 0x40)
#define CONNECT_WILL_RETAIN(flags)       ((flags) & 0x20)
#define CONNECT_WILL_QOS(flags)          (((flags) >> 3) & 0x03)
#define CONNECT_WILL_FLAG(flags)         ((flags) & 0x04)
#define CONNECT_CLEAN_SESSION(flags)     ((flags) & 0x02)
#define CONNECT_RESERVED(flags)          ((flags) & 0x01)

typedef enum connack_return_code_e
{
    CONNECTION_ACCEPTED,
    UNACCEPTABLE_PROTOCOL_VERSION,
    IDENTIFIER_REJECTED,
    SERVER_UNAVAILABLE,
    BAD_USERNAME_OR_PASSWORD,
    NOT_AUTHORIZED
} connack_return_code;

typedef struct tmq_connack_pkt
{
    uint8_t ack_flags;
    connack_return_code return_code;
} tmq_connack_pkt;

typedef struct tmq_publish_pkt
{

} tmq_publish_pkt;

typedef struct tmq_puback_pkt
{

} tmq_puback_pkt;

typedef struct tmq_pubrec_pkt
{

} tmq_pubrec_pkt;

typedef struct tmq_pubrel_pkt
{

} tmq_pubrel_pkt;

typedef struct tmq_pubcomp_pkt
{

} tmq_pubcomp_pkt;

typedef struct topic_filter_qos
{
    tmq_str_t topic_filter;
    uint8_t qos;
} topic_filter_qos;
typedef tmq_vec(topic_filter_qos) topic_list;
typedef struct tmq_subscribe_pkt
{
    uint16_t packet_id;
    topic_list topics;
} tmq_subscribe_pkt;

typedef struct tmq_suback_pkt
{
    uint16_t packet_id;
    tmq_vec(uint8_t) return_codes;
} tmq_suback_pkt;

typedef struct tmq_unsubscribe_pkt
{

} tmq_unsubscribe_pkt;

typedef struct tmq_unsuback_pkt
{

} tmq_unsuback_pkt;

typedef struct tmq_pingreq_pkt
{

} tmq_pingreq_pkt;

typedef struct tmq_pingresp_pkt
{

} tmq_pingresp_pkt;

typedef struct tmq_disconnect_pkt
{

} tmq_disconnect_pkt;

void tmq_connect_pkt_cleanup(tmq_connect_pkt* pkt);
void tmq_subscribe_pkt_cleanup(tmq_subscribe_pkt* pkt);
void tmq_suback_pkt_cleanup(tmq_suback_pkt* pkt);
/* for debug */
void tmq_connect_pkt_print(tmq_connect_pkt* pkt);
void tmq_subsribe_pkt_print(tmq_subscribe_pkt* pkt);

#endif //TINYMQTT_MQTT_PACKET_H
