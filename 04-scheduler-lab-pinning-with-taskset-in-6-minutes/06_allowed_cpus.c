/* Pinning with taskset in 6 Minutes: One Process, One Core - slide 6: a program that prints its allowed cpus (C version of 06_allowed_cpus.cpp) */
/* Build: make 06_allowed_cpus_c */
/* Try:   ./06_allowed_cpus_c    then    taskset -c 2 ./06_allowed_cpus_c    then    taskset 0xd ./06_allowed_cpus_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>

#if defined(__linux__)
#include <errno.h>
#include <sched.h>
#include <string.h>
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
    return 0;                                 /* unknown, as hardware_concurrency may say */
#endif
}

int main(void) {
    printf("logical CPUs the system reports: %ld\n", logical_cpus());

#if defined(__linux__)
    /* Ask the kernel: which CPUs may this process run on? No root needed */
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) != 0) {   /* 0 = myself */
        printf("sched_getaffinity: %s\n", strerror(errno));
        return 1;
    }
    printf("allowed: %d CPUs:", CPU_COUNT(&set));
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
        if (CPU_ISSET(cpu, &set)) printf(" %d", cpu);
    printf("\nrunning on CPU %d\n", sched_getcpu());

    /* the same set as the hex mask taskset prints with -p (first 64 CPUs) */
    unsigned long long mask = 0;
    for (int cpu = 0; cpu < 64; ++cpu)
        if (CPU_ISSET(cpu, &set)) mask |= 1ull << cpu;
    printf("as a hex mask: 0x%llx\n", mask);

    if (CPU_COUNT(&set) == 1)
        printf("pinned: one allowed CPU, the load balancer cannot migrate this process\n");
    else
        printf("not pinned: run me again under  taskset -c 2  and compare\n");
#else
    printf("this sample needs Linux: it would print the CPUs the process is allowed on "
           "(sched_getaffinity) and the CPU it runs on (sched_getcpu), all of them when run plain, "
           "exactly one under taskset -c 2\n");
#endif
    return 0;
}
