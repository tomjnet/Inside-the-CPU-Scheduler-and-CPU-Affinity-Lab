/* Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 6: percentiles: sort once, index four times (C version of 06_percentiles.cpp) */
/* Build: make 06_percentiles_c */
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

#if !defined(__linux__)
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

/* The loop of slide 5: sleep to an absolute deadline, record how late each wakeup is, in nanoseconds.
   Returns a malloc'd array of `samples` values (NULL when out of memory); the caller frees it. */
static long long *measure(int samples) {
    long long *late_ns = malloc((size_t)samples * sizeof *late_ns);
    if (late_ns == NULL) return NULL;
#if defined(__linux__)
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    for (int i = 0; i < samples; ++i) {
        next.tv_nsec += (long)kPeriodNs;
        if (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            ++next.tv_sec;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        late_ns[i] = (long long)(now.tv_sec - next.tv_sec) * 1000000000LL + (now.tv_nsec - next.tv_nsec);
    }
#else
    double next = now_ns();                           /* portable fallback: same grid, sleep until */
    for (int i = 0; i < samples; ++i) {
        next += (double)kPeriodNs;
        sleep_until_ns(next);
        late_ns[i] = (long long)(now_ns() - next);
    }
#endif
    return late_ns;
}

static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}

/* nearest rank on a sorted array: the index is the percentile */
static long long percentile(const long long *sorted, size_t n, double p) {
    double pos = p / 100.0 * (double)n;
    size_t rank = (size_t)pos;
    return sorted[rank < n - 1 ? rank : n - 1];
}

int main(void) {
#if !defined(__linux__)
    printf("this sample needs Linux: clock_nanosleep wakeup latency; using a portable sleep until instead\n");
#endif
    const size_t n = kSamples;
    long long *late = measure(kSamples);              /* 5000 x 1 ms */
    if (late == NULL) {
        printf("malloc failed: %s\n", strerror(ENOMEM));
        return 0;
    }
    qsort(late, n, sizeof late[0], cmp_ll);           /* O(n log n), once */
    long long p50  = percentile(late, n, 50.0);       /* the typical wakeup */
    long long p99  = percentile(late, n, 99.0);       /* 50 samples are worse */
    long long p999 = percentile(late, n, 99.9);       /* 5 samples are worse */
    long long worst = late[n - 1];                    /* the one that hurts */

    printf("wakeup latency of %zu samples, this machine\n", n);
    printf("  p50   : %lld us\n", p50 / 1000);
    printf("  p99   : %lld us   (%zu samples are worse)\n", p99 / 1000, n / 100);
    printf("  p99.9 : %lld us   (%zu samples are worse)\n", p999 / 1000, n / 1000);
    printf("  max   : %lld us\n", worst / 1000);
    if (p50 > 0)
        printf("  tail ratio p99.9 / p50: %lldx\n", p999 / p50);
    printf("report the four together, with the sample count; never average percentiles\n");

    /* correctness: percentiles of a sorted array can only grow */
    int ordered = p50 <= p99 && p99 <= p999 && p999 <= worst;
    printf(ordered ? "check: p50 <= p99 <= p99.9 <= max\n" : "check FAILED: percentiles out of order\n");
    free(late);                                       /* C has no destructor: free on the way out */
    return ordered ? 0 : 1;
}
