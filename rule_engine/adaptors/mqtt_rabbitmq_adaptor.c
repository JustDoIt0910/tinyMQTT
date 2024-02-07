//
// Created by just do it on 2024/2/3.
//
#include "mqtt_adaptors.h"
#include <stdlib.h>

static void register_parameters(adaptor_parameter_map* parameter_map)
{
    add_parameter(*parameter_map, "exchange", ADAPTOR_VALUE_STR);
    add_parameter(*parameter_map, "routingKey", ADAPTOR_VALUE_STR);
}

static void handle_event(adaptor_value_map* parameters, adaptor_value_list* payload)
{
    printf("parameters: \n");
    for(tmq_map_iter_t iter = tmq_map_iter(*parameters); tmq_map_has_next(iter); tmq_map_next(*parameters, iter))
    {
        printf("%s=", (char*)iter.first);
        adaptor_value_t* value = iter.second;
        if(value->value_type == ADAPTOR_VALUE_STR)
            printf("%s\n", value->str);
        else
            printf("%ld\n", value->integer);
    }
    printf("payloads: \n");
    for(adaptor_value_item_t* it = tmq_vec_begin(*payload); it != tmq_vec_end(*payload); it++)
    {
        printf("%s: ", it->value_name);
        if(it->value.value_type == ADAPTOR_VALUE_STR)
            printf("%s\n", it->value.str);
        else
            printf("%ld\n", it->value.integer);
    }
}

typedef struct
{
    ADAPTOR_PUBLIC_MEMBER
} rabbitmq_adaptor;

tmq_adaptor_t* get_rabbitmq_adaptor(tmq_config_t* cfg)
{
    rabbitmq_adaptor* adaptor = malloc(sizeof(rabbitmq_adaptor));
    adaptor->register_parameters = register_parameters;
    adaptor->handle_event = handle_event;
    return (tmq_adaptor_t*)adaptor;
}