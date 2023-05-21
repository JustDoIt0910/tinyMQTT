//
// Created by zr on 23-4-30.
//

#ifndef TINYMQTT_MQTT_PACKET_H
#define TINYMQTT_MQTT_PACKET_H
#include <stdint.h>

typedef enum tmq_packet_type_e
{
    MQTT_CONNECT, MQTT_CONNACK,
    MQTT_PUBLISH, MQTT_PUBACK,
    MQTT_PUBREC, MQTT_PUBREL, MQTT_PUBCOMP,
    MQTT_SUBSCRIBE, MQTT_SUBACK,
    MQTT_UNSUBSCRIBE, MQTT_UNSUBACK,
    MQTT_PINGREQ, MQTT_PINGRESP,
    MQTT_DISCONNECT
} tmq_packet_type_e;

typedef struct tmq_packet_t
{
    tmq_packet_type_e packet_type;
    void* packet;
} tmq_packet_t;

typedef struct tmq_connect_pkt
{

} tmq_connect_pkt;

typedef struct tmq_connack_pkt
{

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

typedef struct tmq_subscribe_pkt
{

} tmq_subscribe_pkt;

typedef struct tmq_suback_pkt
{

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

#endif //TINYMQTT_MQTT_PACKET_H
