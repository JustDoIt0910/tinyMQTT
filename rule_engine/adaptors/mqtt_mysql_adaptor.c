//
// Created by just do it on 2024/2/6.
//
#include "mqtt_adaptors.h"
#include <stdlib.h>

static void register_parameters(adaptor_parameter_map* parameter_map)
{
    add_parameter(parameter_map, "table", ADAPTOR_VALUE_STR);
}

static void handle_event(tmq_adaptor_t* adaptor, adaptor_value_map* parameters, adaptor_value_list* payload_kvs)
{

}

typedef struct
{
    ADAPTOR_PUBLIC_MEMBER
} mysql_adaptor;

tmq_adaptor_t* get_mysql_adaptor(tmq_config_t* cfg, void* arg, tmq_str_t* error)
{
    mysql_adaptor* adaptor = malloc(sizeof(mysql_adaptor));
    adaptor->register_parameters = register_parameters;
    adaptor->handle_event = handle_event;
    return (tmq_adaptor_t*)adaptor;
}