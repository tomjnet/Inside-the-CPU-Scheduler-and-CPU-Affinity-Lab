/* Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 8: three runs: alone, competing, pinned apart (C version of 08_three_runs.cpp) */
/* Build: make 08_three_runs_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, clock_nanosleep */
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

static volatile uint64_t g_sink = 0;
static void sink(uint64_t x) { g_sink = g_sink + x; }   /* keeps the spinner's loop alive */

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

/* the CPUs this process may use (episode 4): never a hard coded number; fills out[0] and out[1] */
static void allowed_cpus(int out[2]) {
    int found = 0;
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) == 0)
        for (int c = 0; c < CPU_SETSIZE && found < 2; ++c)
            if (CPU_ISSET(c, &set)) out[found++] = c;
#endif
    if (found == 0) out[found++] = 0;
    if (found == 1) out[found++] = out[0];                /* one CPU only: run 3 cannot move the spinner away */
}

/* pins the calling thread (episode 5); a refusal is reported and the run continues unpinned */
static void pin_self(int cpu) {
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        printf("  sched_setaffinity(cpu %d) refused: %s\n", cpu, strerror(errno));
#else
    (void)cpu;                                            /* no portable pinning: the fallback runs unpinned */
#endif
}

/* a thread that never sleeps, pinned to one CPU. C has no destructor:
   spinner_start and spinner_stop are the constructor and the destructor, called by hand */
typedef struct {
    atomic_bool stop;
    int cpu;
    int running;
    thrd_t worker;
} Spinner;

static int spinner_run(void *arg) {
    Spinner *s = (Spinner *)arg;
    pin_self(s->cpu);
    while (!atomic_load_explicit(&s->stop, memory_order_relaxed)) sink(1);
    return 0;
}

static void spinner_start(Spinner *s, int cpu) {
    atomic_store(&s->stop, 0);
    s->cpu = cpu;
    s->running = thrd_create(&s->worker, spinner_run, s) == thrd_success;
    if (!s->running) printf("  thrd_create failed: this run has no spinner\n");
}

static void spinner_stop(Spinner *s) {
    atomic_store(&s->stop, 1);
    if (s->running) thrd_join(s->worker, NULL);
    s->running = 0;
}

static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}

static long long percentile(const long long *sorted, size_t n, double p) {
    double pos = p / 100.0 * (double)n;
    size_t rank = (size_t)pos;
    return sorted[rank < n - 1 ? rank : n - 1];
}

/* takes ownership of the array: sorts it, prints one row, frees it */
static void report(const char *name, long long *late, size_t n) {
    if (late == NULL) {
        printf("  %-22s malloc failed: %s\n", name, strerror(ENOMEM));
        return;
    }
    qsort(late, n, sizeof late[0], cmp_ll);
    printf("  %-22s p50 %6lld us  p99 %6lld us  p99.9 %6lld us  max %6lld us\n", name,
           percentile(late, n, 50.0) / 1000, percentile(late, n, 99.0) / 1000,
           percentile(late, n, 99.9) / 1000, late[n - 1] / 1000);
    free(late);
}

int main(void) {
#if !defined(__linux__)
    printf("this sample needs Linux: sched_setaffinity pins the runs; here all three run unpinned\n");
#endif
    int cpus[2];
    allowed_cpus(cpus);
    printf("three runs of %d wakeups, this machine: our CPU %d, the other CPU %d\n", kSamples, cpus[0], cpus[1]);

    /* same loop, three runs: only the neighbour changes */
    pin_self(cpus[0]);                      /* sched_setaffinity, episode 5 */
    report("alone", measure(kSamples), kSamples);

    Spinner noisy;
    spinner_start(&noisy, cpus[0]);         /* competes for our CPU */
    report("spinner on our CPU", measure(kSamples), kSamples);
    spinner_stop(&noisy);                   /* joined here */

    Spinner apart;
    spinner_start(&apart, cpus[1]);         /* same load, another CPU */
    report("spinner pinned apart", measure(kSamples), kSamples);

    printf("read the tail columns: the median often stays close, p99.9 and max show the neighbour\n");
    printf("numbers from WSL or a virtual machine are noisy: repeat on real Linux hardware\n");
    spinner_stop(&apart);                   /* the C++ destructor runs at the end of main: here by hand */
    return 0;
}
