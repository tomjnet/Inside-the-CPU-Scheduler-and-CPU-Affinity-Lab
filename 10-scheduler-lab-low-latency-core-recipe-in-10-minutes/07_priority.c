/* A Repeatable Low Latency Core Recipe in 10 Minutes - slide 7: stage 5: priority, with a safety net (C version of 07_priority.cpp) */
/* Build: make 07_priority_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>

#if defined(__linux__)
#include <errno.h>
#include <sched.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

static long long now_ns(void) {
    struct timespec t = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000000000LL + t.tv_nsec;
}

static int g_polls = 0;
static long long g_worst_ns = 0;
static bool done(void) { return g_polls >= 500; }       /* the work ends on its own, well before the timeout */

/* One unit of latency sensitive work: sleep 1 ms, record how late the wakeup was. */
static void poll_once(void) {
    struct timespec nap = {0, 1000000};
    long long t0 = now_ns();
    clock_nanosleep(CLOCK_MONOTONIC, 0, &nap, NULL);
    long long late = now_ns() - t0 - 1000000;
    if (late > g_worst_ns) g_worst_ns = late;
    ++g_polls;
}

static const char *policy_name(int policy) {
    switch (policy) {
        case SCHED_FIFO: return "SCHED_FIFO";
        case SCHED_RR: return "SCHED_RR";
        case SCHED_OTHER: return "SCHED_OTHER";
        default: return "another policy";
    }
}
#endif

int main(void) {
#if defined(__linux__)
    printf("policy at start: %s\n", policy_name(sched_getscheduler(0)));

    /* Stage 5: real time priority, only with a safety net */
    struct sched_param sp = {0};
    sp.sched_priority = 50;                   /* 1 to 99: 50 is plenty */
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
        printf("sched_setscheduler: %s\n", strerror(errno));  /* EPERM without root */
        puts("continuing as SCHED_OTHER");
    }
    printf("policy in the loop: %s\n", policy_name(sched_getscheduler(0)));
    long long stop = now_ns() + 2000000000LL; /* the timeout: 2 seconds */
    while (now_ns() < stop && !done())        /* never an endless FIFO loop */
        poll_once();
    sp.sched_priority = 0;
    sched_setscheduler(0, SCHED_OTHER, &sp);  /* rollback in the same file */

    printf("%d wakeups, worst overshoot %lld us on this machine\n", g_polls, g_worst_ns / 1000);
    printf("policy at exit: %s\n", policy_name(sched_getscheduler(0)));
    printf("with root (sudo, or CAP_SYS_NICE) the loop runs as SCHED_FIFO 50; the timeout stays either way\n");
#else
    printf("this sample needs Linux: it asks for SCHED_FIFO priority 50 with sched_setscheduler, reports EPERM\n"
           "without root, runs a loop bounded by a 2 second timeout and returns to SCHED_OTHER\n");
#endif
    return 0;
}
