/* Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 7: the histogram: buckets that double (C version of 07_histogram.cpp) */
/* Build: make 07_histogram_c */
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
#define kBuckets 16

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

/* one row: lower edge of the bucket, count, and a bar scaled to 50 columns for all the samples */
static void print_row(long long from_us, int count) {
    int width = count * 50 / kSamples;
    if (count > 0 && width == 0) width = 1;           /* a tail of one sample must stay visible */
    printf("%8lld us | %5d | ", from_us, count);
    for (int i = 0; i < width; ++i) putchar('#');
    putchar('\n');
}

int main(void) {
#if !defined(__linux__)
    printf("this sample needs Linux: clock_nanosleep wakeup latency; using a portable sleep until instead\n");
#endif
    const size_t n = kSamples;
    long long *late = measure(kSamples);
    if (late == NULL) {
        printf("malloc failed: %s\n", strerror(ENOMEM));
        return 0;
    }
    printf("wakeup latency histogram, %zu samples, this machine\n", n);
    printf("    from    | count | each bucket covers twice the range of the previous one\n");

    /* bucket b holds latencies from 2^b to 2^(b+1) microseconds */
    int bucket[kBuckets] = {0};
    for (size_t i = 0; i < n; ++i) {
        long long us = late[i] / 1000;
        size_t b = 0;
        while (us > 1 && b < kBuckets - 1) { us /= 2; ++b; }   /* integer log2 */
        ++bucket[b];
    }
    for (size_t b = 0; b < kBuckets; ++b)             /* one row per bucket */
        print_row(1LL << b, bucket[b]);               /* from, count, bar */
    /* the tail: short rows far below the tallest one */

    int total = 0;
    for (size_t b = 0; b < kBuckets; ++b) total += bucket[b];
    printf("one narrow hump: healthy. a second hump: a periodic disturbance. a long thin tail: rare events\n");
    int complete = total == (int)n;
    printf(complete ? "check: every sample landed in a bucket\n" : "check FAILED: samples lost\n");
    free(late);                                       /* C has no destructor: free on the way out */
    return complete ? 0 : 1;
}
