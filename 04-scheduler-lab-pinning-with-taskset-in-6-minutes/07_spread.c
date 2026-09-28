/* Pinning with taskset in 6 Minutes: One Process, One Core - slide 7: pin the benchmark, compare the spread (C version of 07_spread.cpp) */
/* Build: make 07_spread_c */
/* Try:   ./07_spread_c    then    taskset -c 2 ./07_spread_c    (a few times each, ideally on a busy machine) */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__linux__)
#include <sched.h>
#endif

/* ---- timing harness: the C twin of bench_ns and sink ---- */
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static double now_ns(void) {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1e9 / (double)f.QuadPart;
}
#else
#include <time.h>
static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
#endif

static volatile uint64_t g_sink = 0;
static void sink(uint64_t x) { g_sink = g_sink + x; }   /* keeps the result alive: the loop cannot be deleted */

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* Fixed work: n steps of a xorshift generator. Same input, same steps, every run. */
static uint64_t work(int n) {
    uint64_t x = 88172645463325252ull, sum = 0;
    for (int i = 0; i < n; ++i) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        sum += x;
    }
    return sum;
}

#define RUNS 200
#define MAX_CPUS 1024

int main(void) {
    for (int i = 0; i < 20; ++i) sink(work(200000));   /* warm up: caches, branch predictor, clock speed */

    /* Same work 200 times: the spread is the machine, not the code */
    double ns[RUNS];
    /* C has no std::set: one flag per CPU number marks the CPUs this process visited */
    static bool visited[MAX_CPUS];
    int visited_count = 0;
    for (int run = 0; run < RUNS; ++run) {
        double t0 = now_ns();
        sink(work(200000));                   /* fixed work per run */
        ns[run] = now_ns() - t0;
#if defined(__linux__)
        int cpu = sched_getcpu();             /* pinned: always one CPU */
        if (cpu >= 0 && cpu < MAX_CPUS && !visited[cpu]) {
            visited[cpu] = true;
            ++visited_count;
        }
#endif
    }

    qsort(ns, RUNS, sizeof ns[0], cmp_double);
    const size_t n = RUNS;
    const double lo = ns[0], median = ns[n / 2], p99 = ns[n * 99 / 100], hi = ns[n - 1];
    printf("200 runs of the same work, this machine, microseconds per run\n");
    printf("  min    %g\n", lo / 1000.0);
    printf("  median %g\n", median / 1000.0);
    printf("  p99    %g\n", p99 / 1000.0);
    printf("  max    %g\n", hi / 1000.0);
    printf("  max / median = %g   p99 / median = %g\n", hi / median, p99 / median);

    if (visited_count == 0) {
        printf("CPUs visited: this part needs Linux (sched_getcpu): it would list every CPU the runs "
               "landed on, exactly one under taskset -c 2\n");
    } else {
        printf("CPUs visited: %d (", visited_count);
        bool first = true;
        for (int cpu = 0; cpu < MAX_CPUS; ++cpu) {
            if (!visited[cpu]) continue;
            printf("%s%d", first ? "" : " ", cpu);
            first = false;
        }
        printf(")\n");
        printf("%s", visited_count == 1 ? "one CPU for every run: pinned, or simply never migrated this time\n"
                                        : "migrated between runs: try again under  taskset -c 2\n");
    }
    printf("compare the tail (p99, max), not the median, and repeat: one run proves nothing\n");
    return 0;
}
