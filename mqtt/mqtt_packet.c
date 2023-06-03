//
// Created by zr on 23-4-30.
//
#include "mqtt_packet.h"
#include "tlog.h"

void tmq_connect_pkt_cleanup(tmq_connect_pkt* pkt)
{
    tmq_str_free(pkt->client_id);
    tmq_str_free(pkt->will_topic);
    tmq_str_free(pkt->will_message);
    tmq_str_free(pkt->username);
    tmq_str_free(pkt->password);
}

void tmq_subscribe_pkt_cleanup(tmq_subscribe_pkt* pkt) {tmq_vec_free(pkt->topics);}

void tmq_suback_pkt_cleanup(tmq_suback_pkt* pkt) { tmq_vec_free(pkt->return_codes);}

void tmq_connect_pkt_print(tmq_connect_pkt* pkt)
{
    tmq_str_t s = tmq_str_new("CONNECT{Username Flag:");
    s = CONNECT_USERNAME_FLAG(pkt->flags) ? tmq_str_append_str(s, "Set;"): tmq_str_append_str(s, " Not set;");
    s = tmq_str_append_str(s, " Password Flag:");
    s = CONNECT_PASSWORD_FLAG(pkt->flags) ? tmq_str_append_str(s, "Set;"): tmq_str_append_str(s, " Not set;");
    s = tmq_str_append_str(s, " Will Retain:");
    s = CONNECT_WILL_RETAIN(pkt->flags) ? tmq_str_append_str(s, "Set;"): tmq_str_append_str(s, " Not set;");
    s = tmq_str_append_str(s, " Will QoS=");
    s = tmq_str_append_char(s, CONNECT_WILL_QOS(pkt->flags) + '0');
    s = tmq_str_append_str(s, "; Will Flag:");
    s = CONNECT_WILL_FLAG(pkt->flags) ? tmq_str_append_str(s, "Set;"): tmq_str_append_str(s, " Not set;");
    s = tmq_str_append_str(s, " Clean Session:");
    s = CONNECT_CLEAN_SESSION(pkt->flags) ? tmq_str_append_str(s, "Set;"): tmq_str_append_str(s, " Not set;");

    tmq_str_t keep_alive = tmq_str_parse_int(pkt->keep_alive, 10);
    s = tmq_str_append_str(s, " Keep Alive=");
    s = tmq_str_append_str(s, keep_alive);
    tmq_str_free(keep_alive);

    s = tmq_str_append_str(s, "; ClientID=");
    s = tmq_str_append_str(s, pkt->client_id);
    if(CONNECT_WILL_FLAG(pkt->flags))
    {
        s = tmq_str_append_str(s, "; Will Topic:");
        s = tmq_str_append_str(s, pkt->will_topic);
        s = tmq_str_append_str(s, "; Will Message:");
        s = tmq_str_append_str(s, pkt->will_message);
    }
    if(CONNECT_USERNAME_FLAG(pkt->flags))
    {
        s = tmq_str_append_str(s, "; Username:");
        s = tmq_str_append_str(s, pkt->username);
    }
    if(CONNECT_PASSWORD_FLAG(pkt->flags))
    {
        s = tmq_str_append_str(s, "; Password:");
        s = tmq_str_append_str(s, pkt->password);
    }
    s = tmq_str_append_str(s, "}");
    tlog_info("%s", s);
    tmq_str_free(s);
}

void tmq_subsribe_pkt_print(tmq_subscribe_pkt* pkt)
{
    tmq_str_t s = tmq_str_new("SUBSCRIBE{PacketID=");
    tmq_str_t packet_id = tmq_str_parse_int(pkt->packet_id, 10);
    s = tmq_str_append_str(s, packet_id);
    tmq_str_free(packet_id);
    struct topic_filter_qos* tf = tmq_vec_begin(pkt->topics);
    for(; tf != tmq_vec_end(pkt->topics); tf++)
    {
        s = tmq_str_append_str(s, ", (");
        s = tmq_str_append_str(s, tf->topic_filter);
        s = tmq_str_append_str(s, ", qos=");
        s = tmq_str_append_char(s, (char) (tf->qos + '0'));
        s = tmq_str_append_char(s, ')');
    }
    s = tmq_str_append_char(s, '}');
    tlog_info("%s", s);
    tmq_str_free(s);
}