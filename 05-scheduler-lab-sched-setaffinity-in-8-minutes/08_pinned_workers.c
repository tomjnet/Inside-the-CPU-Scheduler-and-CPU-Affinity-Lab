/* CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 8: the benchmark: four workers, free and pinned (C version of 08_pinned_workers.cpp) */
/* Build: make 08_pinned_workers_c */
/* Usage: ./08_pinned_workers_c [repeats] [pinned]   (09_verify.sh runs the C++ binary; this one takes the same arguments) */
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
#include <string.h>

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
/* the C11 mutex, mapped the same way (inline: unused ones cost no warning) */
typedef pthread_mutex_t mtx_t;
enum { mtx_plain = 0 };
static inline int mtx_init(mtx_t *m, int type) { (void)type; return pthread_mutex_init(m, NULL) == 0 ? thrd_success : thrd_error; }
static inline int mtx_lock(mtx_t *m) { return pthread_mutex_lock(m) == 0 ? thrd_success : thrd_error; }
static inline int mtx_unlock(mtx_t *m) { return pthread_mutex_unlock(m) == 0 ? thrd_success : thrd_error; }
static inline void mtx_destroy(mtx_t *m) { pthread_mutex_destroy(m); }
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

static int g_repeats = 201;                                   /* a couple of hundred runs per worker */

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* warm up, repeat, keep the minimum and the median; the samples live on the heap: any repeat count */
static BenchResult bench_ns(void (*f)(void *), void *ctx, int warmup, int repeats) {
    BenchResult r = { 0.0, 0.0 };
    double *samples = malloc((size_t)repeats * sizeof *samples);
    if (samples == NULL) return r;
    for (int i = 0; i < warmup; ++i) f(ctx);
    for (int i = 0; i < repeats; ++i) {
        double t0 = now_ns();
        f(ctx);
        samples[i] = now_ns() - t0;
    }
    qsort(samples, (size_t)repeats, sizeof samples[0], cmp_double);
    r.min_ns = samples[0];
    r.median_ns = samples[repeats / 2];
    free(samples);
    return r;
}

#if defined(__linux__)
#include <errno.h>
#include <sched.h>
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
/* fills cpus (room for CPU_SETSIZE) and returns how many are allowed */
static int allowed_cpus(int *cpus) {
    int n = 0;
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof allowed, &allowed) != 0) return 0;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
        if (CPU_ISSET(cpu, &allowed)) cpus[n++] = cpu;
    return n;
}
#define MAX_CPUS CPU_SETSIZE
#else
/* stand-ins so the free pass still runs and prints timings on other systems */
static bool pin_self(int target) { (void)target; return false; }
static int current_cpu(void) { return 0; }
static int allowed_cpus(int *cpus) { cpus[0] = 0; return 1; }
#define MAX_CPUS 1
#endif

#define K_WORKERS 4
static const size_t kElements = (size_t)1 << 20;             /* 4 MiB per worker: about a millisecond or less per sum */
static uint32_t *data[K_WORKERS];                             /* C has no std::vector: one malloc per worker */
static mtx_t print_mutex;

static int g_cpus[MAX_CPUS];
static int g_ncpus = 0;

static uint64_t sum_squares(const uint32_t *v, size_t n) {
    uint64_t s = 0;
    for (size_t i = 0; i < n; ++i) s += (uint64_t)v[i] * v[i];
    return s;
}

static void report(size_t id, int last, int moves, BenchResult r) {
    mtx_lock(&print_mutex);                  /* C has no lock_guard: unlock by hand */
    printf("  worker %zu: last CPU %d, moves %d, min %g us, median %g us\n",
           id, last, moves, r.min_ns / 1000.0, r.median_ns / 1000.0);
    mtx_unlock(&print_mutex);
}

typedef struct { size_t id; bool pin; int last; int moves; } WorkerCtx;

static void job(void *arg) {                 /* the lambda of the C++ version */
    WorkerCtx *w = (WorkerCtx *)arg;
    sink(sum_squares(data[w->id], kElements));   /* about 1 ms of work */
    int now = current_cpu();                 /* vDSO: no system call */
    if (now != w->last) { ++w->moves; w->last = now; }
}

/* one worker: time the same job and count the CPU changes */
static int worker(void *arg) {
    WorkerCtx *w = (WorkerCtx *)arg;
    if (w->pin) pin_self(g_cpus[w->id % (size_t)g_ncpus]);   /* one CPU each */
    w->last = current_cpu();
    w->moves = 0;
    BenchResult r = bench_ns(job, w, 3, g_repeats);          /* harness: min and median */
    report(w->id, w->last, w->moves, r);     /* pinned: moves is 0 */
    return 0;
}

static void pass(const char *title, bool pin) {
    printf("%s\n", title);
    fflush(stdout);
    thrd_t threads[K_WORKERS];
    WorkerCtx ctx[K_WORKERS];
    bool started[K_WORKERS];
    for (size_t id = 0; id < K_WORKERS; ++id) {
        ctx[id].id = id;
        ctx[id].pin = pin;
        ctx[id].last = 0;
        ctx[id].moves = 0;
        started[id] = thrd_create(&threads[id], worker, &ctx[id]) == thrd_success;
        if (!started[id]) fprintf(stderr, "thrd_create failed for worker %zu\n", id);
    }
    for (size_t id = 0; id < K_WORKERS; ++id)
        if (started[id]) thrd_join(threads[id], NULL);
}

/* fixed seed splitmix64 in place of std::mt19937: every run does the same work */
static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

int main(int argc, char **argv) {
    if (argc > 1) { g_repeats = atoi(argv[1]); if (g_repeats < 1) g_repeats = 1; }
    bool pinned_only = argc > 2 && strcmp(argv[2], "pinned") == 0;

    int rc = 0;
    uint64_t rng = 12345;
    for (size_t w = 0; w < K_WORKERS; ++w) {
        data[w] = malloc(kElements * sizeof *data[w]);
        if (data[w] == NULL) { fprintf(stderr, "malloc failed\n"); rc = 1; goto done; }
        for (size_t i = 0; i < kElements; ++i) data[w][i] = (uint32_t)(splitmix64(&rng) & 0xFFFFu);
    }
    if (mtx_init(&print_mutex, mtx_plain) != thrd_success) { fprintf(stderr, "mtx_init failed\n"); rc = 1; goto done; }

    g_ncpus = allowed_cpus(g_cpus);
    if (g_ncpus == 0) { g_cpus[0] = 0; g_ncpus = 1; }
    printf("%d allowed CPUs, %d workers, %d timed runs each (this machine)\n", g_ncpus, K_WORKERS, g_repeats);

    if (!pinned_only) pass("free: the scheduler places and moves the workers", false);
#if defined(__linux__)
    pass("pinned: worker i on allowed CPU i, moves must be 0", true);
    printf("quiet machine? the free workers may not move either: start a compile next to it and run again\n");
#else
    printf("this sample needs Linux: the pinned pass with sched_setaffinity and the moves counted by sched_getcpu\n");
#endif
    mtx_destroy(&print_mutex);
done:                                        /* C has no destructor: one cleanup label frees every buffer */
    for (size_t w = 0; w < K_WORKERS; ++w) free(data[w]);
    return rc;
}
