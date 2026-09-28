/* CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 7: startup order: pin first, allocate after (C version of 07_pin_then_touch.cpp) */
/* Build: make 07_pin_then_touch_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ---- threads: C11 <threads.h>; MinGW lacks it, so map the calls onto pthreads ---- */
#if defined(__MINGW32__)
#include <pthread.h>
#include <sched.h>
typedef pthread_t thrd_t;
typedef int (*thrd_start_t)(void *);
enum { thrd_success = 0, thrd_error = 2 };
struct thrd_boot { thrd_start_t fn; void *arg; };
static void *thrd_boot_run(void *p) {
    struct thrd_boot b = *(struct thrd_boot *)p;
    free(p);
    return (void *)(intptr_t)b.fn(b.arg);
}
static int thrd_create(thrd_t *t, thrd_start_t fn, void *arg) {
    struct thrd_boot *b = malloc(sizeof *b);
    if (b == NULL) return thrd_error;
    b->fn = fn;
    b->arg = arg;
    if (pthread_create(t, NULL, thrd_boot_run, b) != 0) { free(b); return thrd_error; }
    return thrd_success;
}
static int thrd_join(thrd_t t, int *res) {
    void *r = NULL;
    if (pthread_join(t, &r) != 0) return thrd_error;
    if (res != NULL) *res = (int)(intptr_t)r;
    return thrd_success;
}
static inline void thrd_yield(void) { sched_yield(); }
#else
#include <threads.h>
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

typedef struct { double min_ns; double median_ns; } BenchResult;

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* warm up, repeat, keep the minimum and the median; at most 64 repeats */
static BenchResult bench_ns(void (*f)(void *), void *ctx, int warmup, int repeats) {
    double samples[64];
    if (repeats > 64) repeats = 64;
    for (int i = 0; i < warmup; ++i) f(ctx);
    for (int i = 0; i < repeats; ++i) {
        double t0 = now_ns();
        f(ctx);
        samples[i] = now_ns() - t0;
    }
    qsort(samples, (size_t)repeats, sizeof samples[0], cmp_double);
    BenchResult r = { samples[0], samples[repeats / 2] };
    return r;
}

#if defined(__linux__)
#include <errno.h>
#include <sched.h>
#include <string.h>
static bool pin_self(int target) {
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(target, &one);
    if (sched_setaffinity(0, sizeof one, &one) != 0) {
        fprintf(stderr, "pin failed: %s\n", strerror(errno));
        return false;
    }
    return sched_getcpu() == target;
}
static int current_cpu(void) { return sched_getcpu(); }
static int pick_cpu(void) {                                   /* second allowed CPU, or the only one */
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof allowed, &allowed) != 0) return 0;
    int cpus[CPU_SETSIZE];
    int n = 0;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
        if (CPU_ISSET(cpu, &allowed)) cpus[n++] = cpu;
    return n == 0 ? 0 : cpus[1 % n];
}
#else
static bool pin_self(int target) {
    (void)target;
    printf("this sample needs Linux: sched_setaffinity before the allocation; the order below still runs, unpinned\n");
    return false;
}
static int current_cpu(void) { return -1; }
static int pick_cpu(void) { return 0; }
#endif

static atomic_bool ready = false;

typedef struct { const uint64_t *data; size_t n; } Span;   /* pointer plus length: C has no std::vector */

static uint64_t sum(const uint64_t *v, size_t n) {
    uint64_t s = 0;
    for (size_t i = 0; i < n; ++i) s += v[i];
    return s;
}

static void sum_job(void *ctx) {                              /* the lambda of the C++ version */
    const Span *s = (const Span *)ctx;
    sink(sum(s->data, s->n));
}

static void run(const uint64_t *data, size_t n) {
    Span s = { data, n };
    BenchResult r = bench_ns(sum_job, &s, 3, 21);
    printf("hot loop on CPU %d: sum of %zu elements, min %g us, median %g us (this machine)\n",
           current_cpu(), n, r.min_ns / 1000.0, r.median_ns / 1000.0);
}

typedef struct { int cpu; size_t n; } WorkerArgs;

static int worker(void *arg) {
    const WorkerArgs *a = (const WorkerArgs *)arg;
    pin_self(a->cpu);                        /* 1. pin before allocating */
    uint64_t *data = malloc(a->n * sizeof *data);   /* 2. allocate and touch: */
    if (data == NULL) {                      /*    pages land near cpu */
        fprintf(stderr, "malloc failed\n");
        atomic_store(&ready, true);          /* let main go on */
        return 1;
    }
    for (size_t i = 0; i < a->n; ++i) data[i] = 1;
    for (int i = 0; i < 3; ++i) sink(sum(data, a->n));   /* 3. warm up */
    atomic_store(&ready, true);              /* 4. only now take work */
    run(data, a->n);                         /* same CPU, warm cache */
    free(data);                              /* C has no destructor: free by hand */
    return 0;
}

int main(void) {
    WorkerArgs args = { pick_cpu(), (size_t)1 << 20 };        /* 8 MiB of uint64_t: 2048 pages to place */
    printf("main runs on CPU %d, worker will pin to CPU %d and only then allocate\n", current_cpu(), args.cpu);
    fflush(stdout);

    thrd_t t;
    if (thrd_create(&t, worker, &args) != thrd_success) {
        fprintf(stderr, "thrd_create failed\n");
        return 1;
    }
    while (!atomic_load(&ready)) thrd_yield();                /* the ready flag rises after pin, touch and warm up */
    int res = 0;
    thrd_join(t, &res);                                       /* print after the join: one writer at a time */
    printf("main saw the ready flag only after the pin, the first touch and the warm up\n");

    printf("on one memory node (a laptop, WSL) the order costs only cache warmth;\n"
           "on two sockets check the placement with: numastat -p <pid>\n");
    return res;
}
