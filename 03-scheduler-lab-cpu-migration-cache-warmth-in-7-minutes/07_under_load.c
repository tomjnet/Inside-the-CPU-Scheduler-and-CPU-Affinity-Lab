/* CPU Migration and Cache Warmth in 7 Minutes - slide 7: the same worker under load (C version of 07_under_load.cpp) */
/* Build: make 07_under_load_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_getcpu, clock_nanosleep */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <errno.h>
#include <sched.h>
#include <stdatomic.h>
#include <threads.h>                 /* threads start only in the Linux branch, where glibc has C11 threads */
#include <time.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* how often two neighbouring samples differ: each change is one wakeup on another CPU */
static int count_changes(const int *trace, size_t n) {
    int changes = 0;
    for (size_t i = 1; i < n; ++i)
        changes += trace[i] != trace[i - 1];
    return changes;
}

/* the C twin of std::thread::hardware_concurrency: 0 when unknown */
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

#if defined(__linux__)
/* the recorder of slide 6: one sched_getcpu() per millisecond, n samples; the caller frees the array */
static int *record_cpus(int n) {
    int *trace = malloc((size_t)n * sizeof *trace);
    if (trace == NULL) return NULL;
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    for (int i = 0; i < n; ++i) {
        trace[i] = sched_getcpu();
        next.tv_nsec += 1000000L;
        if (next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; ++next.tv_sec; }
        int rc;
        do rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        while (rc == EINTR);
    }
    return trace;
}

/* records n samples and returns the number of changes, or -1 when the array cannot be allocated */
static int changes_over(int n) {
    int *trace = record_cpus(n);
    if (trace == NULL) return -1;
    int c = count_changes(trace, (size_t)n);
    free(trace);                                 /* C has no destructor: free on the way out */
    return c;
}

/* a short burst of work that the compiler cannot delete */
static void spin(void) {
    volatile unsigned x = 0;
    for (int i = 0; i < 1000; ++i) x = x + 1;
}

static atomic_bool stop;
static int spinner(void *arg) {
    (void)arg;
    while (!atomic_load(&stop)) spin();
    return 0;
}

/* se.nr_migrations of the main thread from /proc/self/sched, or -1 when the kernel does not expose it */
static long read_nr_migrations(void) {
    FILE *in = fopen("/proc/self/sched", "r");
    if (in == NULL) return -1;
    char line[256];
    long result = -1;
    while (fgets(line, sizeof line, in) != NULL) {
        if (strstr(line, "nr_migrations") == NULL) continue;
        const char *colon = strchr(line, ':');
        if (colon != NULL) {
            char *end = NULL;
            errno = 0;
            long v = strtol(colon + 1, &end, 10);
            if (end != colon + 1 && errno == 0) result = v;
        }
        break;
    }
    fclose(in);
    return result;
}
#endif

int main(void) {
    const unsigned cpus = logical_cpus();
    printf("logical CPUs: %u, threads in the loaded run: 3 spinners + 1 worker\n", cpus);
    if (cpus > 4)
        printf("more CPUs than threads: an idle CPU is easy to find, so the old one is often still free\n");
    else
        printf("as many threads as CPUs or more: the old CPU is often busy when the worker wakes up\n");
    const int model[] = {1, 1, 3, 3, 1};
    printf("model: trace 1 1 3 3 1 has %d changes\n\n",
           count_changes(model, sizeof model / sizeof model[0]));

#if defined(__linux__)
    (void)read_nr_migrations();                  /* first read opens and faults in what it needs */
    const long m0 = read_nr_migrations();

    int idle = changes_over(1000);               /* quiet machine */
    const long m1 = read_nr_migrations();
    atomic_store(&stop, 0);
    thrd_t load[3];                              /* 3 spinners + the worker */
    int started = 0;
    for (int i = 0; i < 3; ++i) {
        if (thrd_create(&load[started], spinner, NULL) == thrd_success) ++started;
        else printf("thrd_create failed: running with %d spinners\n", started);
    }
    int busy = changes_over(1000);               /* bounded: 1 second */
    atomic_store(&stop, 1);
    for (int i = 0; i < started; ++i) thrd_join(load[i], NULL);
    if (idle < 0 || busy < 0) {
        printf("malloc failed: %s\n", strerror(ENOMEM));
        return 0;
    }
    /* old CPU busy at wakeup: the worker lands on another core,
       and every landing starts with a cold L1 and L2 */
    printf("changes: idle %d, loaded %d\n", idle, busy);

    const long m2 = read_nr_migrations();
    if (m0 >= 0 && m1 >= 0 && m2 >= 0)
        printf("se.nr_migrations of this thread (the kernel's own count): idle run +%ld, loaded run +%ld\n",
               m1 - m0, m2 - m1);
    else
        printf("se.nr_migrations: /proc/self/sched is not available on this kernel\n");
    printf("nothing is asserted: the scheduler owes us no particular number, and it differs per run.\n"
           "on real hardware with more load than cores the loaded count is often many times the idle one\n");
#else
    printf("this sample needs Linux: it would record sched_getcpu() for one second on a quiet machine,\n"
           "again next to three spinner threads, and print both counts of CPU changes\n");
#endif
    return 0;
}
