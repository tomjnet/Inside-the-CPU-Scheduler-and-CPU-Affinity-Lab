/* Context Switches in 7 Minutes: The Hidden Cost - slide 6: oversubscription: four threads, one core (C version of 06_oversubscribe.cpp) */
/* Build: make 06_oversubscribe_c */
/*
 * One spinner alone for a second, then four spinners that must share one CPU for a second. Nobody
 * blocks, so every switch is involuntary, and each thread reads its own counters from
 * /proc/thread-self/status. The process restricts itself to one CPU with sched_setaffinity (a later
 * episode explains the call; taskset -c 0 ./06_oversubscribe_c does the same from the shell).
 * Without Linux the threads still run, spread over the free cores, and the counters read 0.
 */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <inttypes.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <errno.h>
#include <sched.h>
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

/* ---- sleep for whole milliseconds: Sleep on Windows, nanosleep elsewhere ---- */
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
#else
#include <time.h>
static void sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}
#endif

typedef struct { long vol; long invol; } Switches;
static Switches read_switches(void) {  /* this thread: one open, one scan */
    Switches s = { 0, 0 };
    FILE *in = fopen("/proc/thread-self/status", "r");
    if (in == NULL) return s;
    char key[64];
    while (fscanf(in, "%63s", key) == 1) {
        if (strcmp(key, "voluntary_ctxt_switches:") == 0) {
            if (fscanf(in, "%ld", &s.vol) != 1) break;
        } else if (strcmp(key, "nonvoluntary_ctxt_switches:") == 0) {
            if (fscanf(in, "%ld", &s.invol) != 1) break;
        }
    }
    fclose(in);                        /* C has no destructor: the file is closed by hand */
    return s;
}

typedef struct { uint64_t spins; long forced; } Report;

/* C has no lambda capture: each spinner gets the stop flag and its own report slot */
typedef struct { atomic_bool *stop; Report *report; } SpinArgs;

static int spinner(void *arg) {
    SpinArgs *sa = arg;
    Switches a = read_switches();        /* the counters are per thread */
    uint64_t spins = 0;
    while (!atomic_load_explicit(sa->stop, memory_order_relaxed)) ++spins;
    sa->report->spins = spins;
    sa->report->forced = read_switches().invol - a.invol;
    return 0;
}

/* The baseline: one spinner, one second, wherever the scheduler puts it. */
static Report spin_alone(void) {
    atomic_bool stop;
    atomic_init(&stop, 0);
    Report report = { 0, 0 };
    SpinArgs sa = { &stop, &report };
    thrd_t t;
    if (thrd_create(&t, spinner, &sa) != thrd_success) {
        printf("thrd_create failed\n");
        return report;
    }
    sleep_ms(1000);
    atomic_store(&stop, 1);
    thrd_join(t, NULL);
    return report;
}

/* Restrict this thread, and every thread it creates afterwards, to the first CPU it is allowed on. */
static int restrict_to_one_cpu(void) {
#if defined(__linux__)
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) {
        printf("sched_getaffinity refused: %s\n", strerror(errno));
        return 0;
    }
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (!CPU_ISSET(cpu, &allowed)) continue;
        cpu_set_t one;
        CPU_ZERO(&one);
        CPU_SET(cpu, &one);
        if (sched_setaffinity(0, sizeof(one), &one) != 0) {
            printf("sched_setaffinity refused: %s\n", strerror(errno));
            return 0;
        }
        printf("process restricted to CPU %d (same as taskset -c %d)\n", cpu, cpu);
        return 1;
    }
    return 0;
#else
    printf("this sample needs Linux: it would squeeze the four spinners onto one CPU and count\n"
           "their forced switches; here they spread over the free cores and the counters read 0\n");
    return 0;
#endif
}

#define N_SPINNERS 4

int main(void) {
    (void)read_switches();               /* warm up: the first read of a /proc file allocates */

    const Report alone = spin_alone();
    printf("alone:     %" PRIu64 " spins, %ld forced switches\n", alone.spins, alone.forced);

    const int one_cpu = restrict_to_one_cpu();

    /* 4 spinners on 1 core: nobody blocks, every switch is forced */
    atomic_bool stop;
    atomic_init(&stop, 0);
    Report reports[N_SPINNERS] = { { 0, 0 } };   /* spins, forced switches */
    SpinArgs args[N_SPINNERS];
    thrd_t pool[N_SPINNERS];
    int started = 0;
    for (int id = 0; id < N_SPINNERS; ++id) {
        args[id].stop = &stop;
        args[id].report = &reports[id];
        if (thrd_create(&pool[id], spinner, &args[id]) != thrd_success) {
            printf("thrd_create failed\n");
            break;
        }
        ++started;
    }
    sleep_ms(1000);                      /* bounded */
    atomic_store(&stop, 1);
    for (int id = 0; id < started; ++id) thrd_join(pool[id], NULL);
    if (started != N_SPINNERS) return 1;

    long total_forced = 0;
    for (int id = 0; id < N_SPINNERS; ++id) {
        const double share = alone.spins == 0 ? 0.0
            : 100.0 * (double)reports[id].spins / (double)alone.spins;
        printf("spinner %d: %" PRIu64 " spins (%g percent of alone), %ld forced switches\n",
               id, reports[id].spins, share, reports[id].forced);
        total_forced += reports[id].forced;
    }
    printf("this machine: %ld forced switches in one second, %s\n", total_forced,
           one_cpu ? "four threads on one CPU" : "threads not restricted to one CPU");
    printf("expect about 25 percent each and a few hundred switches when one CPU is shared\n");
    return 0;
}
