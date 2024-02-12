//
// Created by zr on 23-5-31.
//
#include "md5.h"
#include "b64.h"
#include "mqtt_util.h"
#include <uuid/uuid.h>

char* password_encode(char* pwd)
{
    uint8_t md5_res[16];
    md5String(pwd, md5_res);
    return b64_encode(md5_res, 16);
}

tmq_str_t get_uuid()
{
    uuid_t uuid;
    bzero(uuid, sizeof(uuid));
    uuid_generate(uuid);
    if(uuid_is_null(uuid))
        fatal_error("failed to generate UUID");
    tmq_str_t str = tmq_str_new_len(NULL, 36);
    uuid_unparse(uuid, str);
    return str;
}