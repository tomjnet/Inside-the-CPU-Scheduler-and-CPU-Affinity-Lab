/* A Repeatable Low Latency Core Recipe in 10 Minutes - slide 6: stage 3 and 4: pin, then place memory (C version of 06_pin_place.cpp) */
/* Build: make 06_pin_place_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define kBytes ((size_t)16 << 20)    /* 16 MiB of "ring buffer" for the worker */

#if defined(__linux__)
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <sys/mman.h>

/* Stage 2 stand-in: the second CPU this process is allowed on (the first one is usually housekeeping).
   On your machine, put here the CPU you picked from lscpu -e and thread_siblings_list. */
static int pick_app_cpu(void) {
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof allowed, &allowed) != 0) return 0;
    int first = -1;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (!CPU_ISSET(cpu, &allowed)) continue;
        if (first >= 0) return cpu;
        first = cpu;
    }
    return first < 0 ? 0 : first;
}
#endif

int main(void) {
#if defined(__linux__)
    const int app_cpu = pick_app_cpu();
    printf("before the pin: running on CPU %d, the application CPU will be %d\n", sched_getcpu(), app_cpu);

    /* Stage 3: pin first. Stage 4: then allocate, touch and lock memory */
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(app_cpu, &set);                   /* app_cpu chosen in stage 2 */
    if (sched_setaffinity(0, sizeof set, &set) != 0)
        printf("sched_setaffinity: %s\n", strerror(errno));   /* check, never assume */
    char *ring = malloc(kBytes);              /* malloc does not touch the pages either */
    if (ring == NULL) {
        printf("malloc of %zu bytes failed\n", kBytes);
        return 1;
    }
    for (size_t i = 0; i < kBytes; i += 4096)
        ring[i] = 1;                          /* first touch: local node */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        printf("mlockall: %s\n", strerror(errno));            /* else: no page faults later */
    printf("running on CPU %d\n", sched_getcpu());

    size_t touched = 0;
    for (size_t i = 0; i < kBytes; i += 4096) touched += (size_t)ring[i];
    printf("%zu pages of 4 KiB touched from the pinned core: first touch places them on its NUMA node\n", touched);
    printf("an mlockall error above means a small RLIMIT_MEMLOCK (ulimit -l): raise it, or run as root\n");
    munlockall();                             /* rollback: the sample leaves nothing locked */
    free(ring);                               /* C has no unique_ptr: free by hand */
#else
    char *ring = malloc(kBytes);
    if (ring == NULL) {
        printf("malloc of %zu bytes failed\n", kBytes);
        return 1;
    }
    size_t touched = 0;
    for (size_t i = 0; i < kBytes; i += 4096) {
        ring[i] = 1;                          /* the portable half: allocate without touching, then touch each page */
        touched += (size_t)ring[i];
    }
    printf("%zu pages of 4 KiB touched\n", touched);
    printf("this sample needs Linux: it pins the thread with sched_setaffinity, touches the memory from the\n"
           "pinned core so first touch places it on the local NUMA node, and locks it with mlockall\n");
    free(ring);                               /* C has no unique_ptr: free by hand */
#endif
    return 0;
}
