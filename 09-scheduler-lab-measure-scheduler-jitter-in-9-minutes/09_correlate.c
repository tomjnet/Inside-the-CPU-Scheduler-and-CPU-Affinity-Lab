/* Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 9: correlate: which outliers moved or were preempted (C version of 09_correlate.cpp) */
/* Build: make 09_correlate_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_getcpu, clock_nanosleep */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <errno.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__linux__)
#include <sched.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* ---- threads: C11 <threads.h>; MinGW lacks it, so map the two calls onto pthreads ---- */
#if defined(__MINGW32__)
#include <pthread.h>
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
#else
#include <threads.h>
#endif

#define kSamples 5000                /* 5000 wakeups, 1 ms apart: about 5 seconds */
#define kPeriodNs 1000000LL          /* 1 ms */

static volatile uint64_t g_sink = 0;
static void sink(uint64_t x) { g_sink = g_sink + x; }   /* keeps the loop of the spinners alive */

typedef struct { long long late_ns; int cpu; } Sample;  /* how late, and on which CPU we woke up */
typedef struct { long voluntary; long involuntary; } Switches;

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

/* episode 2: the two context switch counters of this thread, from /proc/self/status (zeros when missing) */
static Switches read_switches(void) {
    Switches s = {0, 0};
    FILE *status = fopen("/proc/self/status", "r");
    if (status == NULL) return s;
    char key[64];
    while (fscanf(status, "%63s", key) == 1) {
        if (strcmp(key, "voluntary_ctxt_switches:") == 0) {
            if (fscanf(status, "%ld", &s.voluntary) != 1) break;
        } else if (strcmp(key, "nonvoluntary_ctxt_switches:") == 0) {
            if (fscanf(status, "%ld", &s.involuntary) != 1) break;
        }
    }
    fclose(status);
    return s;
}

/* the loop of slide 5, with sched_getcpu() after every wakeup.
   Returns a malloc'd array of `samples` entries (NULL when out of memory); the caller frees it. */
static Sample *measure_with_cpu(int samples) {
    Sample *run = malloc((size_t)samples * sizeof *run);
    if (run == NULL) return NULL;
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
        long long late = (long long)(now.tv_sec - next.tv_sec) * 1000000000LL + (now.tv_nsec - next.tv_nsec);
        run[i].late_ns = late;
        run[i].cpu = sched_getcpu();
    }
#else
    double next = now_ns();                           /* portable fallback: no CPU number, always 0 */
    for (int i = 0; i < samples; ++i) {
        next += (double)kPeriodNs;
        sleep_until_ns(next);
        run[i].late_ns = (long long)(now_ns() - next);
        run[i].cpu = 0;
    }
#endif
    return run;
}

static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}

static int spin(void *arg) {
    atomic_bool *stop = (atomic_bool *)arg;
    while (!atomic_load_explicit(stop, memory_order_relaxed)) sink(1);
    return 0;
}

int main(void) {
#if !defined(__linux__)
    printf("this sample needs Linux: sched_getcpu per wakeup and the counters of /proc/self/status\n");
#endif
    /* interference on purpose: three unpinned spinners, so the load balancer has a reason to move us */
    static atomic_bool stop;
    atomic_store(&stop, 0);
    thrd_t spinners[3];
    int started = 0;
    for (int i = 0; i < 3; ++i) {
        if (thrd_create(&spinners[started], spin, (void *)&stop) == thrd_success) ++started;
        else printf("thrd_create failed: running with %d spinners\n", started);
    }

    Switches warm = read_switches();       /* warm up read: /proc itself allocates and can fault in pages */
    long long p99 = 0;                     /* from a short probe run, so the loop below has its threshold */
    Sample *probe = measure_with_cpu(500);
    long long *sorted = malloc(500 * sizeof *sorted);
    Sample *run = NULL;
    int ok = probe != NULL && sorted != NULL;
    if (ok) {
        for (size_t i = 0; i < 500; ++i) sorted[i] = probe[i].late_ns;
        qsort(sorted, 500, sizeof sorted[0], cmp_ll);
        p99 = sorted[500 * 99 / 100];
        printf("warm up: %ld voluntary switches so far, p99 of a 500 sample probe: %lld us\n",
               warm.voluntary, p99 / 1000);
    }
    free(sorted);                          /* C has no destructor: free as soon as the probe is done */
    free(probe);

    /* every sample keeps the CPU it woke up on: sched_getcpu() */
    Switches before = read_switches();     /* /proc/self/status, episode 2 */
    if (ok) run = measure_with_cpu(kSamples);
    Switches after = read_switches();

    atomic_store(&stop, 1);
    for (int i = 0; i < started; ++i) thrd_join(spinners[i], NULL);

    if (run == NULL) {
        printf("malloc failed: %s\n", strerror(ENOMEM));
        return 0;
    }
    int migrations = 0, late_and_moved = 0;
    for (size_t i = 1; i < kSamples; ++i) {
        int moved = run[i].cpu != run[i - 1].cpu;       /* episode 3 */
        if (moved) ++migrations;
        if (moved && run[i].late_ns > p99) ++late_and_moved;
    }
    long preempted = after.involuntary - before.involuntary;
    /* voluntary grows by one per sleep: only involuntary is noise */

    int late_total = 0;
    for (size_t i = 0; i < kSamples; ++i)
        if (run[i].late_ns > p99) ++late_total;
    printf("%d wakeups next to 3 spinners, this machine\n", kSamples);
    printf("  voluntary switches  : %ld   (one per sleep: expected)\n", after.voluntary - before.voluntary);
    printf("  involuntary switches: %ld   (the scheduler took the CPU away)\n", preempted);
    printf("  migrations          : %d   (the wakeup CPU changed)\n", migrations);
    printf("  samples above p99   : %d, of which %d woke on a new CPU\n", late_total, late_and_moved);
    printf("outliers that neither moved nor were preempted: look at /proc/interrupts (episode 8)\n");
    free(run);
    return 0;
}
