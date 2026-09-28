/* CPU Migration and Cache Warmth in 7 Minutes - slide 6: record the cpu each millisecond (C version of 06_record_cpu.cpp) */
/* Build: make 06_record_cpu_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_getcpu, clock_nanosleep */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <errno.h>
#include <sched.h>
#endif

/* ---- timing: the C twin of std::chrono::steady_clock ---- */
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

#define WORDS 32768

/* the working set: reads every word, so the lines must be in the cache of the CPU we run on */
static uint64_t touch(const uint64_t *w, size_t n) {
    uint64_t sum = 0;
    for (size_t i = 0; i < n; ++i) sum += w[i];
    return sum;
}

/* sorts in place: callers pass an array they no longer need in order */
static double median(double *v, size_t n) {
    if (n == 0) return 0.0;
    qsort(v, n, sizeof v[0], cmp_double);
    return v[n / 2];
}

static double touch_ns(const uint64_t *w, size_t n) {
    double t0 = now_ns();
    sink(touch(w, n));
    return now_ns() - t0;
}

#if defined(__linux__)
static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}
#endif

int main(void) {
    /* 256 KiB of 64 bit words: larger than a typical L1, small enough for a typical L2 */
    static uint64_t work[WORDS];
    for (size_t i = 0; i < WORDS; ++i) work[i] = (uint64_t)i * 2654435761ULL;
    printf("working set: %zu KiB, %zu cache lines of 64 bytes\n",
           (size_t)WORDS * sizeof(uint64_t) / 1024, (size_t)WORDS * sizeof(uint64_t) / 64);

    /* portable part: how warm the working set is when nothing moves (touched back to back, no sleep) */
    double warm[21];
    for (int i = 0; i < 3; ++i) sink(touch(work, WORDS));    /* warm up */
    for (int i = 0; i < 21; ++i) warm[i] = touch_ns(work, WORDS);
    printf("one warm touch, back to back: median %g ns (this machine)\n", median(warm, 21));

    /* portable part: what "count the changes" means, on a trace written by hand */
    const int model[] = {2, 2, 2, 5, 5, 5, 2, 2};
    const size_t model_n = sizeof model / sizeof model[0];
    int model_changes = 0;
    for (size_t i = 1; i < model_n; ++i)
        model_changes += model[i] != model[i - 1];
    printf("model trace 2 2 2 5 5 5 2 2: %d CPU changes, 2 distinct CPUs\n\n", model_changes);

#if defined(__linux__)
    enum { SAMPLES = 1000 };
    static double ns[SAMPLES];            /* added: the time of every touch, for the comparison below */
    static int trace[SAMPLES];            /* 1000 samples, 1 ms apart */
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    for (int i = 0; i < SAMPLES; ++i) {
        double t0 = now_ns();
        sink(touch(work, WORDS));         /* keep a working set warm */
        ns[i] = now_ns() - t0;
        trace[i] = sched_getcpu();        /* vDSO: no system call */
        next.tv_nsec += 1000000L;
        if (next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; ++next.tv_sec; }
        int rc;                           /* wakeup = new placement */
        do rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        while (rc == EINTR);
        if (rc != 0) printf("clock_nanosleep: %s\n", strerror(rc));
    }
    int changes = 0;                      /* each change: one cold cache */
    for (size_t i = 1; i < SAMPLES; ++i)
        changes += trace[i] != trace[i - 1];
    printf("CPU changes in 1 s: %d\n", changes);

    int unsupported = 0;
    for (size_t i = 0; i < SAMPLES; ++i) unsupported += trace[i] == -1;
    if (unsupported > 0)
        printf("note: sched_getcpu returned -1 for some samples (not supported here)\n");

    static int sorted[SAMPLES];           /* C has no std::set: sort a copy and skip repeats */
    memcpy(sorted, trace, sizeof trace);
    qsort(sorted, SAMPLES, sizeof sorted[0], cmp_int);
    size_t distinct = 0;
    for (size_t i = 0; i < SAMPLES; ++i)
        if (i == 0 || sorted[i] != sorted[i - 1]) sorted[distinct++] = sorted[i];
    printf("distinct CPUs visited: %zu (", distinct);
    for (size_t i = 0; i < distinct; ++i) printf(" %d", sorted[i]);
    printf(" )\n");

    /* the touch of sample i ran just before sched_getcpu of sample i: if the CPU differs from sample i - 1,
       that touch was the first one on the new CPU */
    static double after_change[SAMPLES];
    static double same_cpu[SAMPLES];
    size_t n_after = 0, n_same = 0;
    for (size_t i = 1; i < SAMPLES; ++i) {
        if (trace[i] != trace[i - 1]) after_change[n_after++] = ns[i];
        else same_cpu[n_same++] = ns[i];
    }
    const double med_same = median(same_cpu, n_same);
    printf("touch after a 1 ms sleep, same CPU:   median %g ns (%zu samples, this machine)\n",
           med_same, n_same);
    if (n_after == 0) {
        printf("touch right after a CPU change:       no change seen in this run\n"
               "  an idle machine often keeps a sleeper on its old CPU: run 07_under_load, or start other load\n");
    } else {
        const double med_after = median(after_change, n_after);
        printf("touch right after a CPU change:       median %g ns (%zu samples, this machine)\n"
               "  ratio %gx: a few samples are noisy, and a hop to an SMT sibling costs almost nothing\n",
               med_after, n_after, med_same > 0.0 ? med_after / med_same : 0.0);
    }
    printf("the count is a lower bound: a hop there and back inside one millisecond is invisible\n");
#else
    printf("this sample needs Linux: it would record sched_getcpu() each millisecond for one second\n"
           "and print the number of CPU changes, the CPUs visited and the touch time after a change\n");
#endif
    return 0;
}
