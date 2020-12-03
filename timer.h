#ifndef SANDBOX_FS_TIMER_H
#define SANDBOX_FS_TIMER_H

#include <ctime>
#include <string>
#include <memory>

namespace T {
static uint64_t now() {
    struct timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000u + ts.tv_nsec;
}

static struct timespec nowts(struct timespec ts = {}) {
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts;
}
}

#endif /* SANDBOX_FS_TIMER_H */
