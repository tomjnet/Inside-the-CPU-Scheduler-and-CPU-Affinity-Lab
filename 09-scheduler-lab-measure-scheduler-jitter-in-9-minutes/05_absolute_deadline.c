/* Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 5: the measurement loop: an absolute deadline (C version of 05_absolute_deadline.cpp) */
/* Build: make 05_absolute_deadline_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* clock_nanosleep, TIMER_ABSTIME */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(__linux__) && defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define kSamples 5000                /* 5000 wakeups, 1 ms apart: about 5 seconds */
#define kPeriodNs 1000000LL          /* 1 ms */

#if defined(__linux__)
static void add_ns(struct timespec *t, long long ns) {
    t->tv_nsec += (long)ns;
    while (t->tv_nsec >= 1000000000L) {
        t->tv_nsec -= 1000000000L;
        ++t->tv_sec;
    }
}

static long long diff_ns(const struct timespec *a, const struct timespec *b) {
    return (long long)(a->tv_sec - b->tv_sec) * 1000000000LL + (a->tv_nsec - b->tv_nsec);
}
#else
/* portable fallback: the C twin of steady_clock::now and sleep_until */
#if defined(_WIN32)
static double now_ns(void) {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1e9 / (double)f.QuadPart;
}
static void sleep_until_ns(double deadline) {          /* never wakes early: whole milliseconds, rounded up */
    for (;;) {
        double left = deadline - now_ns();
        if (left <= 0.0) return;
        Sleep((DWORD)((left + 999999.0) / 1e6));
    }
}
#else
static double now_ns(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
static void sleep_until_ns(double deadline) {
    while (now_ns() < deadline) { }
}
#endif
#endif

int main(void) {
    /* one allocation up front: no allocation inside the loop */
    long long *late_ns = malloc((size_t)kSamples * sizeof *late_ns);
    if (late_ns == NULL) {
        printf("malloc failed: %s\n", strerror(ENOMEM));
        return 0;
    }
    size_t n = 0;

#if defined(__linux__)
    /* ask to wake at an absolute deadline, measure how late we are */
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);         /* start of the grid */
    for (int i = 0; i < kSamples; ++i) {           /* 5000 wakeups */
        add_ns(&next, kPeriodNs);                  /* deadline += 1 ms */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);      /* awake: read the clock */
        late_ns[n++] = diff_ns(&now, &next);       /* wakeup latency */
    }
    /* one voluntary context switch per sample, and no drift */
    printf("clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME), %d wakeups, 1 ms apart\n", kSamples);
#else
    /* portable fallback: the same absolute deadline grid with a sleep until each deadline */
    printf("this sample needs Linux: clock_nanosleep with TIMER_ABSTIME and its wakeup latency\n");
    printf("portable fallback: sleep until each deadline on the same 1 ms grid\n");
    double next = now_ns();
    for (int i = 0; i < kSamples; ++i) {
        next += (double)kPeriodNs;
        sleep_until_ns(next);
        double now = now_ns();
        late_ns[n++] = (long long)(now - next);
    }
#endif

    long long lo = late_ns[0], hi = late_ns[0], sum = 0;
    for (size_t i = 0; i < n; ++i) {
        if (late_ns[i] < lo) lo = late_ns[i];
        if (late_ns[i] > hi) hi = late_ns[i];
        sum += late_ns[i];
    }
    printf("samples: %zu (this machine)\n", n);
    printf("  min wakeup latency: %lld us\n", lo / 1000);
    printf("  avg wakeup latency: %lld us\n", sum / (long long)n / 1000);
    printf("  max wakeup latency: %lld us\n", hi / 1000);
    printf("  jitter (max - min): %lld us\n", (hi - lo) / 1000);
    printf("a normal thread pays timer slack (50 us by default) on top of the scheduler\n");
    printf("min, avg and max hide the shape: 06_percentiles and 07_histogram show it\n");
    free(late_ns);                                 /* C has no destructor: free on the way out */
    return 0;
}
