/* Core Isolation in 9 Minutes: isolcpus and nohz_full - slide 8: cpu lists: who is housekeeping (C version of 08_cpu_list.cpp) */
/* Build: make 08_cpu_list_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* C has no std::vector: a fixed list sized for the parser's 4096 CPU limit */
#define MAX_CPUS 4096
typedef struct { int cpu[MAX_CPUS]; size_t n; } CpuList;

static void push(CpuList *l, int cpu) {
    if (l->n < MAX_CPUS) l->cpu[l->n++] = cpu;
}

static size_t count(const CpuList *l, int cpu) {
    size_t hits = 0;
    for (size_t i = 0; i < l->n; ++i)
        if (l->cpu[i] == cpu) ++hits;
    return hits;
}

static bool is_digit(char ch) { return ch >= '0' && ch <= '9'; }

/* Reads one decimal number at text[*i] and advances *i; false when there is no digit there. */
static bool read_number(const char *text, size_t *i, int *out) {
    if (!is_digit(text[*i])) return false;       /* the '\0' at the end is not a digit */
    *out = 0;
    while (is_digit(text[*i]) && *out < 100000) *out = *out * 10 + (text[(*i)++] - '0');
    return true;
}

/* The kernel CPU list format: "2-3,6" becomes {2, 3, 6}. Anything that is not a digit, a dash or a comma
   ends the parse, so an empty line or "(null)" gives an empty list instead of an error. */
static void parse_cpu_list(const char *text, CpuList *cpus) {
    size_t i = 0;
    int lo = 0;
    cpus->n = 0;
    while (read_number(text, &i, &lo)) {
        int hi = lo;
        if (text[i] == '-') {
            ++i;
            if (!read_number(text, &i, &hi)) break;
        }
        for (int c = lo; c <= hi && c < MAX_CPUS; ++c) push(cpus, c);
        if (text[i] != ',') break;
        ++i;
    }
}

static void print(const char *label, const CpuList *cpus) {
    printf("%s:", label);
    for (size_t i = 0; i < cpus->n; ++i) printf(" %d", cpus->cpu[i]);
    if (cpus->n == 0) printf(" (none)");
    printf("\n");
}

/* static: five lists of 16 KB each would crowd the stack */
static CpuList online, isolated, housekeeping, scratch;

int main(void) {
    /* parse_cpu_list is the episode 4 parser: "2-3,6" becomes {2, 3, 6}. */
    /* Housekeeping = online minus isolated: arithmetic, no system call. */
    parse_cpu_list("0-7", &online);
    parse_cpu_list("2-3,6", &isolated);
    housekeeping.n = 0;
    for (size_t i = 0; i < online.n; ++i)
        if (count(&isolated, online.cpu[i]) == 0)
            push(&housekeeping, online.cpu[i]);  /* 0 1 4 5 7 */
    /* Rule of the lab: CPU 0 always stays a housekeeping CPU. */
    bool cpu0_ok = count(&housekeeping, 0) == 1;

    print("online      ", &online);
    print("isolated    ", &isolated);
    print("housekeeping", &housekeeping);
    printf("CPU 0 is housekeeping: %s\n", cpu0_ok ? "yes" : "NO, fix the boot line");
    printf("left for everything else: %zu of %zu CPUs\n", housekeeping.n, online.n);

    /* Edge cases of the files under /sys: an empty line, a single CPU, a boot line that isolates CPU 0. */
    parse_cpu_list("", &scratch);
    print("empty line  ", &scratch);
    parse_cpu_list("5", &scratch);
    print("single \"5\"  ", &scratch);
    parse_cpu_list("0-3", &scratch);
    printf("isolcpus=0-3 breaks the rule, CPU 0 isolated: %s\n", count(&scratch, 0) == 1 ? "yes" : "no");
    return cpu0_ok ? 0 : 1;
}
