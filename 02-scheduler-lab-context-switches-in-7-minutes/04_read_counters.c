/* Context Switches in 7 Minutes: The Hidden Cost - slide 4: voluntary against involuntary (C version of 04_read_counters.cpp) */
/* Build: make 04_read_counters_c */
/*
 * Reads the two context switch counters Linux keeps for every thread, and proves the detail of the
 * slide: /proc/self/status is the main thread, /proc/thread-self/status is whichever thread asks.
 * The file is plain text, so the reader is portable C; without /proc the counters stay at zero.
 */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* The same scan on any status file, to compare /proc/self with /proc/thread-self. */
static Switches read_switches_from(const char *path) {
    Switches s = { 0, 0 };
    FILE *in = fopen(path, "r");
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

static Switches read_switches(void) {  /* this thread: one open, one scan */
    return read_switches_from("/proc/thread-self/status");
}

static void print(const char *who, Switches s) {
    printf("  %s: voluntary %ld, involuntary %ld\n", who, s.vol, s.invol);
}

/* C has no lambda capture: the worker writes its results into this struct */
typedef struct { Switches delta; Switches self_seen; } WorkerOut;

/* A worker that blocks 50 times. Its switches belong to the worker, not to the main thread. */
static int worker(void *arg) {
    WorkerOut *out = arg;
    (void)read_switches();
    const Switches a = read_switches();
    for (int i = 0; i < 50; ++i) sleep_ms(1);
    const Switches b = read_switches();
    out->delta.vol = b.vol - a.vol;
    out->delta.invol = b.invol - a.invol;
    out->self_seen = read_switches_from("/proc/self/status");   /* still the main thread */
    return 0;
}

int main(void) {
    FILE *probe = fopen("/proc/thread-self/status", "r");
    if (probe == NULL) {
        printf("this sample needs Linux: it would print voluntary_ctxt_switches and\n"
               "nonvoluntary_ctxt_switches of the main thread and of a worker thread\n");
        print("without /proc every counter reads", read_switches());
        return 0;
    }
    fclose(probe);

    (void)read_switches();                     /* warm up: the first read of a /proc file allocates */
    const Switches main_before = read_switches();
    print("main thread, baseline", main_before);

    WorkerOut out = { { 0, 0 }, { 0, 0 } };
    thrd_t t;
    if (thrd_create(&t, worker, &out) != thrd_success) {
        printf("thrd_create failed\n");
        return 1;
    }
    thrd_join(t, NULL);                        /* one voluntary switch of the main thread: it waits here */

    const Switches main_after = read_switches();
    print("worker, 50 sleeps of 1 ms (its own delta)", out.delta);
    print("/proc/self/status read inside the worker", out.self_seen);
    print("main thread, after the join", main_after);
    printf("the worker blocked about 50 times; the main thread only moved by %ld voluntary switch(es)\n",
           main_after.vol - main_before.vol);
    return 0;
}
