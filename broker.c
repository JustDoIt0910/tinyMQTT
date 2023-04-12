//
// Created by zr on 23-4-9.
//
#include "tlog.h"
#include <stdio.h>
#include "mqtt_map.h"

struct Value
{
    int a;
    int b;
};

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tmq_map(char*, struct Value) m = tmq_map_str(struct Value, 10, 75);

    struct Value v= {1, 2};
    tmq_map_put(m, "key", v);
    struct Value* res = tmq_map_get(m, "key");
    if(res)
        printf("{%d, %d}\n", res->a, res->b);

    tlog_exit();
    return 0;
}