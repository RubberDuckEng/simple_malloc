#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

#include "malloc.h"

int main(int argc, const char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <size>\n", argv[0]);
        return -1;
    }
    init_heap();

    struct timespec monotime;
    clock_gettime(CLOCK_MONOTONIC, &monotime);

    int64_t t1 = monotime.tv_sec * 1000000000 + monotime.tv_nsec;

    const int reps = atoi(argv[1]);
    const int size = 100;

    void *ptrs[reps];
    for (int i = 0; i < reps; i++)
    {
        ptrs[i] = my_malloc(size);
    }
    for (int i = 0; i < reps; i++)
    {
        my_free(ptrs[i]);
    }

    // nanosleep((const struct timespec[]){{0, 1000000}}, NULL);

    clock_gettime(CLOCK_MONOTONIC, &monotime);
    int64_t t2 = monotime.tv_sec * 1000000000 + monotime.tv_nsec;

    int64_t delta = t2 - t1;
    printf("time delta=%" PRId64 "ns\n", delta);

    return 0;
}
