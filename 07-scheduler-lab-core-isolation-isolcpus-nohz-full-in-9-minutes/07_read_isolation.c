/* Core Isolation in 9 Minutes: isolcpus and nohz_full - slide 7: read the same files in c++ (C version of 07_read_isolation.cpp) */
/* Build: make 07_read_isolation_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* First line of a small text file into buf, newline removed. False when the file is missing. */
static bool read_first_line(const char *path, char *buf, size_t size) {
    FILE *in = fopen(path, "r");
    if (in == NULL) return false;
    buf[0] = '\0';
    if (fgets(buf, (int)size, in) != NULL) buf[strcspn(buf, "\n")] = '\0';
    fclose(in);                                  /* C has no destructor: close by hand */
    return true;
}

/* True when the boot line holds "<name>=" as the start of a parameter. */
static bool has_parameter(const char *cmdline, const char *name) {
    char key[64];
    snprintf(key, sizeof key, "%s=", name);
    const char *at = strstr(cmdline, key);
    while (at != NULL) {
        if (at == cmdline || at[-1] == ' ') return true;
        at = strstr(at + 1, key);
    }
    return false;
}

int main(void) {
#if !defined(__linux__)
    printf("this sample needs Linux: it would print /proc/cmdline and the isolated and nohz_full CPU lists\n"
           "the loop still runs, so every file reads as (not available):\n");
#endif
    /* Isolation state is three text files: open, read, close, once each. */
    const char *files[] = {
        "/proc/cmdline",                         /* the boot line itself */
        "/sys/devices/system/cpu/isolated",      /* what isolcpus did */
        "/sys/devices/system/cpu/nohz_full",     /* CPUs without the tick */
    };
    for (size_t i = 0; i < sizeof files / sizeof files[0]; ++i) {
        char line[4096];
        if (!read_first_line(files[i], line, sizeof line))
            snprintf(line, sizeof line, "(not available)");   /* no file: no feature */
        else if (line[0] == '\0')
            snprintf(line, sizeof line, "(none)");
        printf("%s: %s\n", files[i], line);
    }

    /* The same boot line, searched for the three parameters of the episode. */
    char cmdline[4096] = "";
    if (!read_first_line("/proc/cmdline", cmdline, sizeof cmdline)) cmdline[0] = '\0';
    const char *names[] = {"isolcpus", "nohz_full", "rcu_nocbs"};
    const size_t name_count = sizeof names / sizeof names[0];
    printf("\nboot parameters of this machine:\n");
    for (size_t i = 0; i < name_count; ++i)
        printf("  %s: %s\n", names[i], has_parameter(cmdline, names[i]) ? "present" : "absent");

    /* The search on a typical lab boot line, so the output teaches on any machine. */
    const char *typical = "BOOT_IMAGE=/vmlinuz ro quiet splash isolcpus=2-3 nohz_full=2-3 rcu_nocbs=2-3";
    printf("typical lab boot line: %s\n", typical);
    for (size_t i = 0; i < name_count; ++i)
        printf("  %s: %s\n", names[i], has_parameter(typical, names[i]) ? "present" : "absent");
    return 0;
}
