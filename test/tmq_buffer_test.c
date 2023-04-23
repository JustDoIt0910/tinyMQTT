//
// Created by zr on 23-4-22.
//
#include <stddef.h>
#include <stdio.h>

size_t get_index(size_t x)
{
    return (((x - 1) >> 9) > 0) + (((x - 1) >> 10) > 0) + (((x - 1) >> 11) > 0) + (((x - 1) >> 12) > 0);
}

size_t sizes[] = {512, 513, 600, 1023, 1024, 1025, 2000, 2048, 2049, 4000, 4096, 4097, 65536};

int main()
{
    for(int i = 0; i < 13; i++)
        printf("%lu => %lu\n", sizes[i], get_index(sizes[i]));
    return 0;
}