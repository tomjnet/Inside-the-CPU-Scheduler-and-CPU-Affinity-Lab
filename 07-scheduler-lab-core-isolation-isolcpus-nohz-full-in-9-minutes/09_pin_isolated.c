/* Core Isolation in 9 Minutes: isolcpus and nohz_full - slide 9: run on the isolated core (C version of 09_pin_isolated.cpp) */
/* Build: make 09_pin_isolated_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if defined(__linux__)
#include <sched.h>
#endif

static const char *kIsolated = "/sys/devices/system/cpu/isolated";

/* First line of a small text file into buf, empty when the file is missing. */
static const char *read_line(const char *path, char *buf, size_t size) {
    buf[0] = '\0';
    FILE *in = fopen(path, "r");
    if (in == NULL) return buf;
    if (fgets(buf, (int)size, in) != NULL) buf[strcspn(buf, "\n")] = '\0';
    else buf[0] = '\0';
    fclose(in);
    return buf;
}

/* First CPU of a kernel CPU list ("2-3,6" gives 2), -1 when the list is empty. */
static int first_cpu(const char *list) {
    if (list[0] < '0' || list[0] > '9') return -1;   /* also catches the empty string */
    int cpu = 0;
    for (const char *ch = list; *ch != '\0'; ++ch) {
        if (*ch < '0' || *ch > '9' || cpu > 100000) break;
        cpu = cpu * 10 + (*ch - '0');
    }
    return cpu;
}

#if defined(__linux__)
/* Highest CPU this thread may use (a container or cgroup can allow fewer than the machine has). */
static int last_allowed_cpu(void) {
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) return 0;
    int last = 0;
    for (int c = 0; c < CPU_SETSIZE; ++c)
        if (CPU_ISSET(c, &allowed)) last = c;
    return last;
}
#endif

int main(void) {
    char line[4096];
    printf("first_cpu(\"2-3,6\") = %d, first_cpu(\"\") = %d\n", first_cpu("2-3,6"), first_cpu(""));
#if defined(__linux__)
    printf("started on CPU %d, isolated list: \"%s\"\n", sched_getcpu(), read_line(kIsolated, line, sizeof line));

    /* isolcpus keeps everyone out, this program included: ask for the core. */
    int cpu = first_cpu(read_line(kIsolated, line, sizeof line));   /* -1: nothing isolated */
    if (cpu < 0) cpu = last_allowed_cpu();       /* fallback for the demo */
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);                          /* exactly one CPU */
    /* One system call and one migration, paid once at startup. */
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        perror("sched_setaffinity");             /* EINVAL: CPU not allowed */
    printf("asked for CPU %d, running on CPU %d\n", cpu, sched_getcpu());

    if (first_cpu(read_line(kIsolated, line, sizeof line)) < 0)
        printf("nothing is isolated here: on the lab machine with isolcpus=2-3 this asks for CPU 2\n");
#else
    printf("this sample needs Linux: it would pin itself to the first CPU of %s (here: \"%s\") with sched_setaffinity\n",
           kIsolated, read_line(kIsolated, line, sizeof line));
#endif
    return 0;
}
