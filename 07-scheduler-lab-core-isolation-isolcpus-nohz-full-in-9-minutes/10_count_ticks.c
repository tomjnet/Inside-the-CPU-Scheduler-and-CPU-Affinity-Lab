/* Core Isolation in 9 Minutes: isolcpus and nohz_full - slide 10: count the ticks (C version of 10_count_ticks.cpp) */
/* Build: make 10_count_ticks_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
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
#include <sched.h>
#endif

/* ---- timing: now_ns() from the canonical harness ---- */
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

/* C has no std::vector: a heap array plus its length, freed by the caller with counters_free */
typedef struct { long long *v; size_t n; } Counters;

static void counters_free(Counters *c) {
    free(c->v);
    c->v = NULL;
    c->n = 0;
}

/* The counters of one /proc/interrupts line: "LOC:  250  1000  3  Local timer interrupts" gives {250, 1000, 3}. */
static Counters parse_counters(const char *line) {
    Counters counters = {NULL, 0};
    const char *colon = strchr(line, ':');
    if (colon == NULL) return counters;
    size_t cap = 0;
    const char *p = colon + 1;
    for (;;) {
        char *end = NULL;
        long long value = strtoll(p, &end, 10);
        if (end == p) break;                                    /* stops at the first word of the description */
        if (counters.n == cap) {
            size_t new_cap = cap == 0 ? 16 : cap * 2;
            long long *grown = realloc(counters.v, new_cap * sizeof *grown);
            if (grown == NULL) break;
            counters.v = grown;
            cap = new_cap;
        }
        counters.v[counters.n++] = value;
        p = end;
    }
    return counters;
}

/* One whole line of any length into *buf (grown with realloc), newline removed; 0 at end of file. */
static int read_full_line(FILE *in, char **buf, size_t *cap) {
    size_t len = 0;
    if (*buf == NULL) {
        *cap = 1024;
        *buf = malloc(*cap);
        if (*buf == NULL) return 0;
    }
    for (;;) {
        if (fgets(*buf + len, (int)(*cap - len), in) == NULL) return len > 0;
        len += strlen(*buf + len);
        if (len > 0 && (*buf)[len - 1] == '\n') { (*buf)[len - 1] = '\0'; return 1; }
        char *grown = realloc(*buf, *cap * 2);
        if (grown == NULL) return 1;                            /* keep what fits */
        *buf = grown;
        *cap *= 2;
    }
}

/* One counter per CPU from the line that starts with `name` in /proc/interrupts, empty when there is none. */
static Counters read_line_named(const char *name) {
    Counters found = {NULL, 0};
    FILE *in = fopen("/proc/interrupts", "r");
    if (in == NULL) return found;
    char *line = NULL;
    size_t cap = 0;
    while (read_full_line(in, &line, &cap)) {
        const char *start = line + strspn(line, " ");
        if (strncmp(start, name, strlen(name)) == 0) {
            found = parse_counters(line);
            break;
        }
    }
    free(line);                                                 /* C has no destructor: free and close by hand */
    fclose(in);
    return found;
}

static Counters read_loc_line(void) { return read_line_named("LOC:"); }

/* A bounded busy loop: keeps this CPU busy until the deadline, never longer. */
static void spin_for(double ns) {
    double deadline = now_ns() + ns;
    uint64_t n = 0;
    while (now_ns() < deadline) sink(++n);
}

int main(void) {
#if defined(__linux__)
    /* Stay on the CPU we started on, so the two reads count the same core. */
    int here = sched_getcpu();
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(here < 0 ? 0 : here, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) printf("could not pin, the CPU may change\n");
    Counters warm = read_loc_line();
    printf("warm up read: %zu LOC columns\n", warm.n);        /* page faults happen here */
    counters_free(&warm);

    Counters hvs_before = read_line_named("HVS:");
    /* LOC in /proc/interrupts: local timer interrupts, one column per CPU. */
    /* The spin keeps our CPU busy: an idle CPU stops its tick anyway. */
    Counters before = read_loc_line();
    spin_for(1e9);                               /* bounded busy loop, one second */
    Counters after = read_loc_line();
    size_t cpu = (size_t)sched_getcpu();
    Counters hvs_after = read_line_named("HVS:");
    if (cpu >= before.n || cpu >= after.n) {
        printf("no LOC column for CPU %zu in /proc/interrupts: nothing to count here\n", cpu);
        goto done;
    }
    printf("CPU %zu: %lld timer interrupts in one second\n", cpu, after.v[cpu] - before.v[cpu]);
    /* Housekeeping CPU: about HZ, 250 or 1000. nohz_full CPU running */
    /* only this task: a handful. Same program, different core. */

    if (after.v[cpu] - before.v[cpu] == 0 && cpu < hvs_before.n && cpu < hvs_after.n)
        printf("LOC is zero: this is a Hyper-V guest (WSL) and its tick is the HVS line: "
               "%lld in one second. Take the real number on hardware.\n",
               hvs_after.v[cpu] - hvs_before.v[cpu]);
    printf("on real hardware expect about HZ (250 or 1000) on a housekeeping CPU, a handful on a "
           "nohz_full CPU\n");
done:
    counters_free(&hvs_before);
    counters_free(&before);
    counters_free(&after);
    counters_free(&hvs_after);
#else
    printf("this sample needs Linux: it would count the LOC timer interrupts of its own busy CPU\n");
    /* The parser and the arithmetic still run, on two typical lines taken one second apart. */
    Counters before = parse_counters("LOC:  881200  450100  9031  Local timer interrupts");
    spin_for(100e6);                             /* 100 ms */
    Counters after = parse_counters("LOC:  881450  450350  9033  Local timer interrupts");
    Counters here = read_loc_line();
    printf("/proc/interrupts here: %zu LOC columns\n", here.n);
    for (size_t cpu = 0; cpu < before.n && cpu < after.n; ++cpu)
        printf("typical CPU %zu: %lld timer interrupts in one second%s\n", cpu, after.v[cpu] - before.v[cpu],
               cpu == 2 ? "  (nohz_full)" : "  (housekeeping, HZ=250)");
    counters_free(&here);
    counters_free(&before);
    counters_free(&after);
#endif
    return 0;
}
