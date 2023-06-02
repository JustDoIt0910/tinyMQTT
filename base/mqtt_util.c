//
// Created by zr on 23-5-31.
//
#include "md5.h"
#include "b64.h"

char* password_encode(char* pwd)
{
    uint8_t md5_res[16];
    md5String(pwd, md5_res);
    return b64_encode(md5_res, 16);
}