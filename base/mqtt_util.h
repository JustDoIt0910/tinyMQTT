//
// Created by zr on 23-5-18.
//

#ifndef TINYMQTT_MQTT_UTIL_H
#define TINYMQTT_MQTT_UTIL_H
#include "tlog.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>

#define atomicSet(var, value)       __atomic_store_n(&(var), (value), __ATOMIC_SEQ_CST)
#define atomicGet(var)              __atomic_load_n (&(var), __ATOMIC_SEQ_CST)
#define atomicExchange(var, val)    __atomic_exchange_n(&(var), val, __ATOMIC_SEQ_CST)
#define decrementAndGet(var, val)   __atomic_sub_fetch(&(var), val, __ATOMIC_SEQ_CST)
#define incrementAndGet(var, val)   __atomic_add_fetch(&(var), val, __ATOMIC_SEQ_CST)

#define mqtt_tid syscall(SYS_gettid)

#define fatal_error(fmt, ...)           \
do {                                    \
    tlog_fatal(fmt, ##__VA_ARGS__);    \
    tlog_exit();                        \
    abort();                            \
}while(0)

char* password_encode(char* pwd);

#endif //TINYMQTT_MQTT_UTIL_H
