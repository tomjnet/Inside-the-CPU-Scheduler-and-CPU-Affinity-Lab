/* The Linux Scheduler in 5 Minutes: Who Runs Next? - slide 6: which cpu am i on? sched_getcpu (C version of 06_which_cpu.cpp) */
/* Build: make 06_which_cpu_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>

#if defined(__linux__)
#include <sched.h>
#include <time.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* C has no std::thread::hardware_concurrency: ask the operating system */
static unsigned logical_cpus(void) {
#if defined(__linux__)
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (unsigned)n : 0u;
#elif defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (unsigned)si.dwNumberOfProcessors;
#else
    return 0u;
#endif
}

int main(void) {
    /* portable part: how many logical CPUs the scheduler can choose from */
    unsigned cpus = logical_cpus();
    printf("logical CPUs: %u, so %u run queues\n", cpus, cpus);

#if defined(__linux__)
    /* which CPU runs this thread? ask 10 times, 100 ms apart */
    int last = -1, moves = 0;
    for (int i = 0; i < 10; ++i) {
        int cpu = sched_getcpu();        /* a snapshot, no system call */
        if (last >= 0 && cpu != last) ++moves;  /* migrated since last look */
        printf("sample %d: CPU %d\n", i, cpu);
        last = cpu;
        struct timespec d = { 0, 100000000L };
        nanosleep(&d, NULL);             /* sleep, wakeup, new pick */
    }

    printf("the CPU changed %d times in 10 samples on this machine\n", moves);
    printf("each sample is a sleep, a wakeup and a new placement decision\n");
    printf("try again while another terminal runs something heavy\n");
#else
    printf("this sample needs Linux: it would print the CPU of this thread\n");
    printf("10 times, 100 ms apart, and count how often the scheduler moved it\n");
#endif
    return 0;
}
