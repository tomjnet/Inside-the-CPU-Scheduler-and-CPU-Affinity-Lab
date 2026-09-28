/* Context Switches in 7 Minutes: The Hidden Cost - slide 5: sleep loop against spin loop (C version of 05_sleep_vs_spin.cpp) */
/* Build: make 05_sleep_vs_spin_c */
/*
 * A loop that sleeps gives the core away every time (voluntary switches); a loop that spins never
 * does, so only the scheduler can take the core from it (involuntary switches). The counters come
 * from /proc/thread-self/status; without /proc (Windows) they stay at zero and the loops still run.
 */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- timing: now_ns of the canonical harness, plus a millisecond sleep ---- */
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static double now_ns(void) {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1e9 / (double)f.QuadPart;
}
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
#else
#include <time.h>
static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
static void sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}
#endif

static volatile uint64_t g_sink = 0;
static void sink(uint64_t x) { g_sink = g_sink + x; }   /* keeps the result alive: the loop cannot be deleted */

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

int main(void) {
    FILE *probe = fopen("/proc/thread-self/status", "r");
    const int has_proc = probe != NULL;
    if (probe != NULL) fclose(probe);
    if (!has_proc)
        printf("this sample needs Linux: it would show about 200 voluntary switches for the\n"
               "sleep loop and none for the spin loop; the loops run anyway, the counters read 0\n");
    (void)read_switches();               /* warm up: the first read of a /proc file allocates */

    /* sleeping gives the core away: one voluntary switch per sleep */
    Switches a = read_switches();
    for (int i = 0; i < 200; ++i)
        sleep_ms(1);
    Switches b = read_switches();        /* vol: about 200 more */

    /* spinning never blocks: only the scheduler can take the core */
    const double end = now_ns() + 300e6;
    uint64_t x = 0;
    while (now_ns() < end) ++x;
    Switches c = read_switches();        /* vol flat, invol: a few */
    sink(x);

    printf("sleep loop, 200 x 1 ms: voluntary +%ld, involuntary +%ld\n",
           b.vol - a.vol, b.invol - a.invol);
    printf("spin loop, 300 ms:      voluntary +%ld, involuntary +%ld  (%" PRIu64 " iterations)\n",
           c.vol - b.vol, c.invol - b.invol, x);
    printf("this machine: involuntary switches appear only when another task wants this core\n");
    return 0;
}
