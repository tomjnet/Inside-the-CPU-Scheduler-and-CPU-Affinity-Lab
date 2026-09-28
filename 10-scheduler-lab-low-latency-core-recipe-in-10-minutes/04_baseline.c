/* A Repeatable Low Latency Core Recipe in 10 Minutes - slide 4: stage 1: the baseline probe (C version of 04_baseline.cpp) */
/* Build: make 04_baseline_c */
/* Run:   ./04_baseline_c [samples]   (default 2000, one wakeup per millisecond; use 5000 or more for a real baseline) */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}

#if defined(__linux__)
#include <time.h>

static long long to_ns(const struct timespec *t) { return (long long)t->tv_sec * 1000000000LL + t->tv_nsec; }
static struct timespec now_ts(void) {
    struct timespec t = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t;
}
static long long now_ns(void) {
    struct timespec t = now_ts();
    return to_ns(&t);
}
static void add_ns(struct timespec *t, long ns) {
    t->tv_nsec += ns;
    while (t->tv_nsec >= 1000000000L) {
        t->tv_nsec -= 1000000000L;
        ++t->tv_sec;
    }
}

/* Stage 1, the episode 9 probe: how late does a 1 ms wakeup arrive? The caller owns late[n]. */
static void measure(long long *late, size_t n) {
    struct timespec next = now_ts();          /* CLOCK_MONOTONIC */
    for (size_t i = 0; i < n; ++i) {
        add_ns(&next, 1000000);               /* absolute deadline: +1 ms */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        late[i] = now_ns() - to_ns(&next);    /* overshoot of this wakeup */
    }
    qsort(late, n, sizeof late[0], cmp_ll);   /* sorted: percentile = index */
}
static const char *kProbe = "clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)";
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static long long now_ns(void) {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (long long)((double)c.QuadPart * 1e9 / (double)f.QuadPart);
}
/* sleep until an absolute deadline: Sleep takes milliseconds and the default timer is coarse */
static void sleep_until_ns(long long deadline) {
    for (;;) {
        long long left = deadline - now_ns();
        if (left <= 0) return;
        Sleep((DWORD)((left + 999999) / 1000000));
    }
}

/* Portable stand-in for the slide's probe: the same absolute deadlines with QueryPerformanceCounter. The default
   Windows timer is coarse (up to about 15 ms), so expect a much larger overshoot here than on Linux. */
static void measure(long long *late, size_t n) {
    long long next = now_ns();
    for (size_t i = 0; i < n; ++i) {
        next += 1000000;                      /* absolute deadline: +1 ms */
        sleep_until_ns(next);
        late[i] = now_ns() - next;
    }
    qsort(late, n, sizeof late[0], cmp_ll);   /* sorted: percentile = index */
}
static const char *kProbe = "Sleep until a QueryPerformanceCounter deadline (this sample needs Linux for the clock_nanosleep probe)";
#endif

/* percentile of a sorted array, in microseconds */
static long long at(const long long *late, size_t n, size_t permille) { return late[(n - 1) * permille / 1000] / 1000; }

int main(int argc, char **argv) {
    size_t n = 2000;
    if (argc > 1) {
        long asked = atol(argv[1]);
        if (asked >= 10 && asked <= 600000) n = (size_t)asked;
    }
    printf("probe: %s\n", kProbe);
    printf("samples: %zu wakeups, one per millisecond, nothing tuned\n", n);

    long long *late = malloc(n * sizeof *late);   /* allocate before the loop */
    if (late == NULL) {
        printf("malloc of %zu samples failed\n", n);
        return 1;
    }
    measure(late, n);

    printf("baseline on this machine: p50 %lld us, p99 %lld us, p99.9 %lld us, max %lld us\n",
           at(late, n, 500), at(late, n, 990), at(late, n, 999), at(late, n, 1000));
    printf("write this line in the log before you change anything: every later stage is compared with it\n");
    free(late);                                   /* C has no destructor: free by hand */
    return 0;
}
