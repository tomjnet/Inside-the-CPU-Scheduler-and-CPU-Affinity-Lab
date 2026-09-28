/* Priorities, Real Time Classes and Preemption in 9 Minutes - slide 6: inspect the scheduling policy (C version of 06_inspect_policy.cpp) */
/* Build: make 06_inspect_policy_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>

#if defined(__linux__)
#include <sched.h>
#include <time.h>

static const char *name_of(int policy) {
    switch (policy) {
        case SCHED_OTHER: return "SCHED_OTHER";
        case SCHED_FIFO: return "SCHED_FIFO";
        case SCHED_RR: return "SCHED_RR";
        case SCHED_BATCH: return "SCHED_BATCH";
        case SCHED_IDLE: return "SCHED_IDLE";
        default: return "another policy (SCHED_DEADLINE is 6)";
    }
}
#endif

int main(void) {
    printf("policies: SCHED_OTHER, SCHED_BATCH, SCHED_IDLE (fair class, nice), "
           "SCHED_FIFO, SCHED_RR (real time, 1..99), SCHED_DEADLINE\n");
#if defined(__linux__)
    /* Which policy am I? One system call each, no root needed. */
    int policy = sched_getscheduler(0);            /* 0 = calling thread */
    struct sched_param sp = {0};
    sched_getparam(0, &sp);                        /* 0 in the fair class */
    int lo = sched_get_priority_min(SCHED_FIFO);   /* 1 on Linux */
    int hi = sched_get_priority_max(SCHED_FIFO);   /* 99 on Linux */
    struct timespec slice = {0};
    sched_rr_get_interval(0, &slice);              /* the RR time slice */
    printf("%s, priority %d, FIFO range %d..%d\n", name_of(policy), sp.sched_priority, lo, hi);

    printf("SCHED_RR range %d..%d, SCHED_OTHER range %d..%d (nice is the knob there)\n",
           sched_get_priority_min(SCHED_RR), sched_get_priority_max(SCHED_RR),
           sched_get_priority_min(SCHED_OTHER), sched_get_priority_max(SCHED_OTHER));
    printf("sched_rr_get_interval: %lld ms (the round robin slice under SCHED_RR, the fair slice otherwise)\n",
           (long long)slice.tv_sec * 1000 + (long long)slice.tv_nsec / 1000000);
    printf("try as root: chrt -r 20 ./06_inspect_policy_c, and watch the first line change\n");
#else
    printf("this sample needs Linux: sched_getscheduler would show SCHED_OTHER, priority 0, FIFO range 1..99\n");
#endif
    return 0;
}
