/* A Repeatable Low Latency Core Recipe in 10 Minutes - slide 9: measure again: the before and after report (C version of 09_report.cpp) */
/* Build: make 09_report_c */
/* Takes about 10 seconds: two runs of 5000 wakeups, one per millisecond. */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define kSamples ((size_t)5000)

static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}

#if defined(__linux__)
#include <errno.h>
#include <sched.h>
#include <string.h>
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

/* The probe of slide 4 (04_baseline.c). The caller owns late[n]. */
static void measure(long long *late, size_t n) {
    struct timespec next = now_ts();
    for (size_t i = 0; i < n; ++i) {
        add_ns(&next, 1000000);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        late[i] = now_ns() - to_ns(&next);
    }
    qsort(late, n, sizeof late[0], cmp_ll);
}

/* Stage 2 stand-in: the second CPU this process is allowed on; use the CPU you picked from the topology. */
static int pick_app_cpu(void) {
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof allowed, &allowed) != 0) return 0;
    int first = -1;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (!CPU_ISSET(cpu, &allowed)) continue;
        if (first >= 0) return cpu;
        first = cpu;
    }
    return first < 0 ? 0 : first;
}

static void pin_to(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof set, &set) != 0) printf("sched_setaffinity: %s\n", strerror(errno));
    printf("the one change: pinned to CPU %d, now running on CPU %d\n", cpu, sched_getcpu());
}
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static long long now_ns(void) {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (long long)((double)c.QuadPart * 1e9 / (double)f.QuadPart);
}
static void sleep_until_ns(long long deadline) {
    for (;;) {
        long long left = deadline - now_ns();
        if (left <= 0) return;
        Sleep((DWORD)((left + 999999) / 1000000));
    }
}

/* Portable stand-in for the probe: the same absolute deadlines with QueryPerformanceCounter (coarse timer on Windows). */
static void measure(long long *late, size_t n) {
    long long next = now_ns();
    for (size_t i = 0; i < n; ++i) {
        next += 1000000;
        sleep_until_ns(next);
        late[i] = now_ns() - next;
    }
    qsort(late, n, sizeof late[0], cmp_ll);
}
static int pick_app_cpu(void) { return 0; }
static void pin_to(int cpu) {
    printf("this sample needs Linux: pin_to(%d) would call sched_setaffinity; here both rows are baselines\n", cpu);
}
#endif

/* Measure again: same probe, same n, one change between the rows */
static long long at(const long long *s, size_t n, size_t permille) {   /* s is sorted: O(1) each */
    return s[(n - 1) * permille / 1000] / 1000;                        /* ns to us */
}
static void report(const char *stage, const long long *s, size_t n) {
    printf("%-9s p50 %5lld p99 %5lld p99.9 %5lld max %5lld us\n",
           stage, at(s, n, 500), at(s, n, 990), at(s, n, 999), at(s, n, 1000));
}

int main(void) {
    const int app_cpu = pick_app_cpu();
    printf("two runs of 5000 wakeups, one per millisecond: about 10 seconds\n");

    long long *before = malloc(kSamples * sizeof *before);
    long long *after = malloc(kSamples * sizeof *after);
    if (before == NULL || after == NULL) {
        printf("malloc of the sample arrays failed\n");
        free(before);
        free(after);
        return 1;
    }
    measure(before, kSamples);                /* stage 1: the baseline */
    pin_to(app_cpu);                          /* exactly one change */
    measure(after, kSamples);                 /* same probe, same n */
    report("baseline", before, kSamples);
    report("pinned", after, kSamples);

    printf("numbers of this machine, this load, this run: on an idle machine the pin may change little\n");
    printf("add one row per stage (memory, priority, isolation, IRQs) and keep a stage only if the tail moved\n");
    free(before);                             /* C has no destructor: free by hand */
    free(after);
    return 0;
}
