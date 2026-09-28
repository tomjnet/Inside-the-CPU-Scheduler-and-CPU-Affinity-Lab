/* CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 3: cpu_set_t: one bit per cpu (C version of 03_cpu_set.cpp) */
/* Build: make 03_cpu_set_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>

#if defined(__linux__)
#include <sched.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* C has no std::thread::hardware_concurrency: ask the operating system directly */
static long logical_cpus(void) {
#if defined(__linux__)
    return sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (long)si.dwNumberOfProcessors;
#else
    return 0;
#endif
}

int main(void) {
    /* portable: the number of logical CPUs of the machine, not the number this thread may use */
    printf("hardware_concurrency: %ld logical CPUs\n", logical_cpus());

#if defined(__linux__)
    /* cpu_set_t: a fixed bitmap, one bit per logical CPU (1024 bits) */
    cpu_set_t allowed;
    CPU_ZERO(&allowed);                          /* macro: no system call */
    /* ask the kernel where this thread may run: one system call */
    if (sched_getaffinity(0, sizeof allowed, &allowed) != 0)
        perror("sched_getaffinity");

    int cpus[CPU_SETSIZE];                       /* the allowed CPU numbers: a fixed array, no VLA */
    int ncpus = 0;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
        if (CPU_ISSET(cpu, &allowed)) cpus[ncpus++] = cpu;

    printf("%d CPUs allowed\n", CPU_COUNT(&allowed));

    printf("sizeof(cpu_set_t): %zu bytes = %d bits\n", sizeof(cpu_set_t), CPU_SETSIZE);
    printf("allowed list:");
    for (int i = 0; i < ncpus; ++i) printf(" %d", cpus[i]);
    printf("\n");

    /* the macros are plain bit operations on the set: build one by hand and test it */
    cpu_set_t mine;
    CPU_ZERO(&mine);
    if (ncpus > 0) CPU_SET(cpus[0], &mine);
    if (ncpus > 1) CPU_SET(cpus[ncpus - 1], &mine);
    printf("hand built set: %d bits on, first allowed CPU is %s\n", CPU_COUNT(&mine),
           (ncpus > 0 && CPU_ISSET(cpus[0], &mine)) ? "set" : "clear");
    printf("try: taskset -c 0,1 ./03_cpu_set_c   (the allowed list shrinks to 0 1)\n");
#else
    printf("this sample needs Linux: the allowed CPU list of this thread from sched_getaffinity\n");
#endif
    return 0;
}
