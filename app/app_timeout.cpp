#include "app_timeout.h"

static struct timespec spec;
static long end;

void set_timeout()
{
    clock_gettime (CLOCK_MONOTONIC, &spec);

    long now;
    now = spec.tv_sec*1000 + spec.tv_nsec/1.0e6;
    end = now + TIMEOUT_MS;
}

int is_timeout()
{
    clock_gettime (CLOCK_MONOTONIC, &spec);
    
    return (spec.tv_sec*1000 + spec.tv_nsec/1.0e6 >= end);
}
