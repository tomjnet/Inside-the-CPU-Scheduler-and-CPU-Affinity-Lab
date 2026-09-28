/* Pinning with taskset in 6 Minutes: One Process, One Core - slide 3: cpu lists and hex masks, in code (C version of 03_list_to_mask.cpp) */
/* Build: make 03_list_to_mask_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* "0,2-3" -> bits 0, 2 and 3 -> 0xd: one bit per logical CPU */
/* C has no istringstream or stoi: strtol walks the text and reports where it stopped */
static uint64_t list_to_mask(const char *list) {
    uint64_t mask = 0;
    const char *p = list;
    while (*p != '\0') {
        char *end = NULL;
        long lo = strtol(p, &end, 10), hi = lo;       /* strtol stops at '-' or ',' */
        if (*end == '-') hi = strtol(end + 1, &end, 10);   /* "2-3" or just "5" */
        for (long cpu = lo; cpu <= hi; ++cpu) mask |= (uint64_t)1 << cpu;
        p = (*end == ',') ? end + 1 : end;
        if (end == p && *p != '\0') break;            /* not a number: stop instead of looping */
    }
    return mask;                                      /* this model: 64 CPUs */
}

/* The way back, as the slide promises: 0xd -> "0,2-3". Consecutive set bits become one range. */
/* C has no std::string: the caller owns the buffer, snprintf appends into it */
static const char *mask_to_list(uint64_t mask, char *out, size_t cap) {
    size_t len = 0;
    out[0] = '\0';
    for (int cpu = 0; cpu < 64; ++cpu) {
        if (((mask >> cpu) & 1u) == 0) continue;
        int last = cpu;
        while (last + 1 < 64 && ((mask >> (last + 1)) & 1u) != 0) ++last;
        int w;
        if (last > cpu)
            w = snprintf(out + len, cap - len, "%s%d-%d", len != 0 ? "," : "", cpu, last);
        else
            w = snprintf(out + len, cap - len, "%s%d", len != 0 ? "," : "", cpu);
        if (w < 0 || (size_t)w >= cap - len) break;   /* buffer full: stop, never overrun */
        len += (size_t)w;
        cpu = last;
    }
    return out;
}

int main(void) {
    const char *lists[] = {"0-7", "0,2-3", "2", "1", "0,4", "2,4,6", "0-3,8-11", "63"};
    int bad = 0;
    char back[256];
    printf("list          hex mask            back to a list\n");
    for (size_t i = 0; i < sizeof lists / sizeof lists[0]; ++i) {
        const char *text = lists[i];
        uint64_t mask = list_to_mask(text);
        mask_to_list(mask, back, sizeof back);
        char hex[24];
        snprintf(hex, sizeof hex, "0x%" PRIx64, mask);
        printf("%-14s%-20s%s\n", text, hex, back);
        if (strcmp(back, text) != 0) ++bad;
    }

    /* the classic mistake of slide 4: without -c, taskset reads a hex mask */
    printf("\ntaskset 2 ./app     is mask 0x2: CPU %s, not CPU 2\n", mask_to_list(0x2, back, sizeof back));
    printf("taskset -c 2 ./app  is mask 0x%" PRIx64 ": CPU 2\n", list_to_mask("2"));
    printf("taskset 0xd ./app   is the list %s\n", mask_to_list(0xd, back, sizeof back));

    if (bad != 0) {
        printf("%d round trips differ\n", bad);
        return 1;
    }
    printf("every list survived the round trip\n");
    return 0;
}
