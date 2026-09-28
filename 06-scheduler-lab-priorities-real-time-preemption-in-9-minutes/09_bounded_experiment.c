/* Priorities, Real Time Classes and Preemption in 9 Minutes - slide 9: a bounded priority experiment (C version of 09_bounded_experiment.cpp) */
/* Build: make 09_bounded_experiment_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdint.h>
#include <stdio.h>

#if defined(__linux__)
#include <errno.h>
#include <inttypes.h>
#include <sched.h>
#include <stdbool.h>
#include <string.h>
#include <sys/resource.h>
#include <threads.h>                 /* the threads only start in the Linux branch */
#include <time.h>
#endif

/* 1.25 to the power n by a loop: the same number as pow, no -lm needed */
static double pow125(int n) {
    double r = 1.0;
    int k = n < 0 ? -n : n;
    for (int i = 0; i < k; ++i) r *= 1.25;
    return n < 0 ? 1.0 / r : r;
}

/* The weight model of slide 3: what the fair class should give two busy tasks on one core. */
static double weight(int nice) { return 1024.0 / pow125(nice); }
static double share(int a, int b) { return weight(a) / (weight(a) + weight(b)); }

#if defined(__linux__)
static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* Slide 7: ask, report EPERM, continue. */
static bool go_real_time(int prio) {
    struct sched_param sp = {0};
    sp.sched_priority = prio;
    if (sched_setscheduler(0, SCHED_FIFO, &sp) == 0) return true;
    printf("  SCHED_FIFO refused: %s, staying SCHED_OTHER\n", strerror(errno));
    return false;
}
static void back_to_normal(void) {
    struct sched_param sp = {0};
    sched_setscheduler(0, SCHED_OTHER, &sp);
}

/* Episode 5: pin the calling thread to one CPU. */
static void pin_to(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof set, &set) != 0)
        printf("  pinning to CPU %d refused: %s\n", cpu, strerror(errno));
}

static int first_allowed_cpu(void) {
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof set, &set) != 0) return 0;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
        if (CPU_ISSET(cpu, &set)) return cpu;
    return 0;
}

/* Two spinners share ONE core until `stop`: who gets the cycles? */
static uint64_t spin(int cpu, int nice_value, bool fifo, double stop) {
    pin_to(cpu);                               /* episode 5 */
    setpriority(PRIO_PROCESS, 0, nice_value);  /* per thread on Linux */
    if (fifo) go_real_time(50);                /* EPERM: stays fair */
    uint64_t n = 0;
    while (now_ns() < stop) ++n;               /* the safety timeout */
    back_to_normal();                          /* always undo */
    return n;                                  /* count = CPU share */
}
/* fair threads trade context switches; a FIFO thread takes no turns */

/* C has no lambda: the thread gets its arguments and its result slot in one struct */
typedef struct {
    int cpu;
    int nice_value;
    bool fifo;
    double stop;
    uint64_t count;
} Spinner;

static int spin_thread(void *arg) {
    Spinner *s = (Spinner *)arg;
    s->count = spin(s->cpu, s->nice_value, s->fifo, s->stop);
    return 0;
}

static void round_of(const char *title, int cpu, int nice_a, int nice_b, bool fifo_a) {
    printf("%s\n", title);
    double stop = now_ns() + 500e6;            /* bounded: both threads stop here */
    Spinner a = {cpu, nice_a, fifo_a, stop, 0};
    Spinner b = {cpu, nice_b, false, stop, 0};
    thrd_t ta, tb;
    bool b_started = thrd_create(&tb, spin_thread, &b) == thrd_success;   /* the fair one starts first */
    bool a_started = thrd_create(&ta, spin_thread, &a) == thrd_success;
    if (a_started) thrd_join(ta, NULL);
    if (b_started) thrd_join(tb, NULL);
    if (!a_started || !b_started) printf("  thrd_create failed: this round is incomplete\n");
    double total = (double)(a.count + b.count);
    if (total <= 0.0) total = 1.0;
    printf("  thread A: %12" PRIu64 " iterations, %5.1f %%\n", a.count, 100.0 * (double)a.count / total);
    printf("  thread B: %12" PRIu64 " iterations, %5.1f %%   (this machine)\n", b.count,
           100.0 * (double)b.count / total);
}
#endif

int main(void) {
    printf("weight model: nice 0 against nice 0 gives %.1f %%, nice 0 against nice 19 gives %.1f %% against %.1f %%\n",
           100.0 * share(0, 0), 100.0 * share(0, 19), 100.0 * share(19, 0));
#if defined(__linux__)
    int cpu = first_allowed_cpu();
    printf("two threads pinned to CPU %d, 500 ms per round, the deadline is inside the loop\n", cpu);
    round_of("round 1: A nice 0, B nice 0 (expect about half each)", cpu, 0, 0, false);
    round_of("round 2: A nice 0, B nice 19 (expect B to get a percent or two)", cpu, 0, 19, false);
    round_of("round 3: A asks for SCHED_FIFO 50, B fair nice 0", cpu, 0, 0, true);
    printf("round 3 with root: A takes about 95 %%, B lives on the throttling reserve;\n"
           "without root: EPERM above and an even split. Take numbers on real Linux hardware.\n");
#else
    printf("this sample needs Linux: two threads pinned to one core would show the split for "
           "nice 0 against nice 19 and for SCHED_FIFO against fair\n");
#endif
    return 0;
}
