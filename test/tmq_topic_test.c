//
// Created by zr on 23-6-5.
//
#include "mqtt/mqtt_topic.h"

int main()
{
    tmq_topics_t topics;
    tmq_topics_init(&topics, NULL);

    tmq_topics_add_subscription(&topics, "/test//topic", "", 0);
}