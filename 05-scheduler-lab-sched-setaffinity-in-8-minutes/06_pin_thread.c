/* CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 6: pin a std::thread through native_handle (C version of 06_pin_thread.cpp) */
/* Build: make 06_pin_thread_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>

/* C has no std::thread and no native_handle(): pthread_create hands out the pthread_t directly */

static atomic_int first_cpu = -1;            /* where the worker ran before the pin reached it */
static atomic_int final_cpu = -1;            /* where it ran once its mask held one CPU */
static atomic_int final_count = 0;

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static int allowed_count(void) {
    cpu_set_t mine;
    CPU_ZERO(&mine);
    if (sched_getaffinity(0, sizeof mine, &mine) != 0) return 0;
    return CPU_COUNT(&mine);
}

/* the worker knows nothing about affinity: it spins until somebody narrows its mask, one second at most */
static void *work(void *arg) {
    (void)arg;
    atomic_store(&first_cpu, sched_getcpu());
    double deadline = now_ns() + 1e9;
    while (allowed_count() != 1 && now_ns() < deadline) sched_yield();
    atomic_store(&final_count, allowed_count());
    atomic_store(&final_cpu, sched_getcpu());
    return NULL;
}

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
#endif

int main(void) {
#if defined(__linux__)
    static int cpus[CPU_SETSIZE];
    int ncpus = allowed_cpus(cpus);
    if (ncpus == 0) {
        printf("sched_getaffinity gave no CPUs: nothing to pin to\n");
        return 0;
    }
    int wanted = cpus[1 % ncpus];

    /* pin another thread: pthread_create gives its pthread_t */
    pthread_t worker;
    int err = pthread_create(&worker, NULL, work, NULL);
    if (err != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(err));
        return 0;
    }
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpus[1 % ncpus], &set);              /* second allowed CPU */
    /* returns the error number itself and leaves errno alone */
    err = pthread_setaffinity_np(worker,
                                 sizeof set, &set);
    if (err != 0) fprintf(stderr, "pin: %s\n", strerror(err));
    /* the worker started before the pin: it may have run elsewhere */
    pthread_join(worker, NULL);

    printf("main thread still has %d allowed CPUs: the mask belongs to a thread\n", allowed_count());
    printf("worker started on CPU %d (before the pin arrived)\n", atomic_load(&first_cpu));
    printf("worker ended on CPU %d with %d allowed CPU, wanted CPU %d: %s\n", atomic_load(&final_cpu),
           atomic_load(&final_count), wanted, atomic_load(&final_cpu) == wanted ? "ok" : "not pinned");
    printf("cleaner: pass the CPU to the worker and let its first line call sched_setaffinity(0, ...)\n");
#else
    printf("this sample needs Linux: pthread_setaffinity_np on the pthread_t of a new thread\n");
#endif
    return 0;
}
