/* Priorities, Real Time Classes and Preemption in 9 Minutes - slide 3: nice values: a weight, not a guarantee (C version of 03_nice.cpp) */
/* Build: make 03_nice_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>

#if defined(__linux__)
#include <sys/resource.h>
#endif

/* 1.25 to the power n by a loop: the same number as pow, no -lm needed */
static double pow125(int n) {
    double r = 1.0;
    int k = n < 0 ? -n : n;
    for (int i = 0; i < k; ++i) r *= 1.25;
    return n < 0 ? 1.0 / r : r;
}

/* nice: -20 (greedy) to 19 (kind), a weight inside the fair class.
   One step is about 1.25x: nice 0 = 1024, nice 19 = about 15. */
static double weight(int nice) { return 1024.0 / pow125(nice); }
static double share(int a, int b) {     /* two busy tasks, one core */
    return weight(a) / (weight(a) + weight(b));
}

#if defined(__linux__)
static void be_kind(void) {
    int before = getpriority(PRIO_PROCESS, 0);   /* 0 = this thread */
    setpriority(PRIO_PROCESS, 0, before + 10);   /* kinder: no root */
    /* going back down needs CAP_SYS_NICE: one system call each */
}
#endif

int main(void) {
    static const int nices[] = {-20, -10, -5, 0, 5, 10, 19};
    static const int pairs[][2] = {{0, 0}, {0, 1}, {0, 5}, {0, 10}, {0, 19}, {-20, 19}};

    printf("the weight model (the kernel table is close to it, rounded by hand):\n");
    for (size_t i = 0; i < sizeof nices / sizeof nices[0]; ++i)
        printf("  nice %3d  weight %8.1f\n", nices[i], weight(nices[i]));

    printf("share of one core, two busy tasks:\n");
    for (size_t i = 0; i < sizeof pairs / sizeof pairs[0]; ++i)
        printf("  nice %3d against nice %2d:  %5.1f %% against %5.1f %%\n", pairs[i][0], pairs[i][1],
               100.0 * share(pairs[i][0], pairs[i][1]), 100.0 * share(pairs[i][1], pairs[i][0]));

#if defined(__linux__)
    int was = getpriority(PRIO_PROCESS, 0);
    be_kind();
    int now = getpriority(PRIO_PROCESS, 0);
    printf("this thread: nice %d before be_kind(), nice %d after\n", was, now);
    if (setpriority(PRIO_PROCESS, 0, was) != 0)
        printf("going back to nice %d was refused: that needs CAP_SYS_NICE or RLIMIT_NICE\n", was);
    else
        printf("going back to nice %d was allowed: you are root or RLIMIT_NICE permits it\n", was);
#else
    printf("this sample needs Linux: getpriority and setpriority would show this thread go from nice 0 to 10\n");
#endif
    printf("a share of the CPU, not a latency guarantee\n");
    return 0;
}
