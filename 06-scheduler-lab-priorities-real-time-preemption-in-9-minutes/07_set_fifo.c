/* Priorities, Real Time Classes and Preemption in 9 Minutes - slide 7: ask for sched_fifo and handle eperm (C version of 07_set_fifo.cpp) */
/* Build: make 07_set_fifo_c */
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

/* SCHED_FIFO needs root, CAP_SYS_NICE or RLIMIT_RTPRIO: else EPERM. */
static bool go_real_time(int prio) {           /* 1..98, never 99 */
    struct sched_param sp = {0};
    sp.sched_priority = prio;
    if (sched_setscheduler(0, SCHED_FIFO, &sp) == 0) return true;
    printf("refused: %s, staying SCHED_OTHER\n", strerror(errno));   /* report and continue */
    return false;
}
static void back_to_normal(void) {
    struct sched_param sp = {0};               /* priority 0: fair */
    sched_setscheduler(0, SCHED_OTHER, &sp);   /* always undo */
}

static void show(const char *when) {
    struct sched_param sp = {0};
    sched_getparam(0, &sp);
    int policy = sched_getscheduler(0);
    const char *name = policy == SCHED_FIFO ? "SCHED_FIFO" : policy == SCHED_OTHER ? "SCHED_OTHER" : "another policy";
    printf("%s: %s, priority %d\n", when, name, sp.sched_priority);
}
#endif

int main(void) {
    printf("asking for SCHED_FIFO priority 50 (the middle: 99 belongs to kernel threads)\n");
#if defined(__linux__)
    show("before");
    bool granted = go_real_time(50);
    show("after the request");
    if (granted)
        printf("granted: you are root, or have CAP_SYS_NICE, or ulimit -r allows 50\n");
    else
        printf("with sudo that line would read SCHED_FIFO, priority 50; the program continues either way\n");
    back_to_normal();
    show("after back_to_normal");
#else
    printf("this sample needs Linux: sched_setscheduler would report EPERM without root, then continue\n");
#endif
    return 0;
}
