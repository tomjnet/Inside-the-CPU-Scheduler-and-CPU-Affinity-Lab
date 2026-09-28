/* CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 4: sched_setaffinity: pin the calling thread (C version of 04_pin_self.cpp) */
/* Build: make 04_pin_self_c */
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
#include <stdbool.h>
#include <string.h>

static bool pin_self(int target) {           /* target: an allowed CPU */
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(target, &one);
    /* pid 0 means the calling thread: one system call, at startup */
    if (sched_setaffinity(0, sizeof one, &one) != 0) {
        /* EINVAL: no allowed CPU in the mask; ESRCH: no such thread */
        fprintf(stderr, "pin failed: %s\n", strerror(errno));
        return false;                        /* keep running, unpinned */
    }
    return sched_getcpu() == target;         /* migrated before return */
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
    printf("allowed CPUs: %d, running on CPU %d\n", ncpus, sched_getcpu());

    /* a CPU this machine does not have: the call is refused and the program keeps running */
    printf("pin to CPU 1023 (expect EINVAL on stderr): ");
    fflush(stdout);
    bool refused = !pin_self(CPU_SETSIZE - 1);
    printf("%s\n", refused ? "refused, still unpinned" : "accepted: this machine is very large");

    /* the real pin: the last CPU of the allowed set, never a fixed number */
    int target = cpus[ncpus - 1];
    bool ok = pin_self(target);
    printf("pin to CPU %d: %s, sched_getcpu() = %d\n", target, ok ? "ok" : "failed", sched_getcpu());
    printf("allowed CPUs now: %d\n", allowed_cpus(cpus));
#else
    printf("this sample needs Linux: sched_setaffinity(0, ...) pins the calling thread, sched_getcpu() confirms it\n");
#endif
    return 0;
}
