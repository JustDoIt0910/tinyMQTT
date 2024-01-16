//
// Created by zr on 23-5-31.
//
#include "md5.h"
#include "b64.h"
#include "mqtt_str.h"
#include "mqtt_util.h"

char* password_encode(char* pwd)
{
    uint8_t md5_res[16];
    md5String(pwd, md5_res);
    return b64_encode(md5_res, 16);
}