//
// Created by zr on 23-4-9.
//
#include "tlog.h"

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tlog_exit();
    return 0;
}