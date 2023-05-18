//
// Created by zr on 23-5-18.
//

#ifndef TINYMQTT_MQTT_UTIL_H
#define TINYMQTT_MQTT_UTIL_H
#include "tlog.h"
#include <unistd.h>
#include <sys/syscall.h>

#define mqtt_tid syscall(SYS_gettid)

#define fatal_error(fmt, ...)           \
do {                                    \
    tlog_fatal(fmt, ##__VA_ARGS__);    \
    tlog_exit();                        \
    abort();                            \
}while(0)

#endif //TINYMQTT_MQTT_UTIL_H
