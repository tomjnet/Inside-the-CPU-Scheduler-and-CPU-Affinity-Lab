/* IRQ Affinity and Core Hygiene in 8 Minutes - slide 7: parse one row of /proc/interrupts in c++ (C version of 07_parse_row.cpp) */
/* Build: make 07_parse_row_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* C has no std::vector: the row keeps a caller owned array of ncpu counters plus a count */
typedef struct { char label[32]; uint64_t *cpu; size_t n; } Row;

/* Next whitespace separated word of *p: its start and length, 0 at the end of the line. */
static size_t next_word(const char **p, const char **start) {
    const char *s = *p;
    while (*s == ' ' || *s == '\t' || *s == '\r') ++s;
    *start = s;
    while (*s != '\0' && *s != ' ' && *s != '\t' && *s != '\r') ++s;
    *p = s;
    return (size_t)(s - *start);
}

/* "25:  0  0  34759  0  PCI-MSIX  eth0-rx-1" becomes one Row.
   Cost: string work only, no system call; the read was one open. */
static void parse_row(const char *line, size_t ncpu, Row *r) {
    const char *p = line, *w;
    size_t len = next_word(&p, &w);                 /* "25:" or "LOC:" */
    if (len >= sizeof r->label) len = sizeof r->label - 1;
    memcpy(r->label, w, len);
    r->label[len] = '\0';
    r->n = 0;
    while (r->n < ncpu && next_word(&p, &w) > 0) {
        if (*w < '0' || *w > '9') break;            /* stops at the chip name */
        r->cpu[r->n++] = strtoull(w, NULL, 10);
    }                                               /* ERR, MIS: fewer than ncpu */
}

/* The header line is "CPU0 CPU1 CPU2": one word per online CPU. */
static size_t count_cpus(const char *header) {
    const char *p = header, *w;
    size_t n = 0;
    while (next_word(&p, &w) > 0) ++n;
    return n;
}

/* Parses every line after the header, prints the kept rows and counts the dropped ones.
   The text is modified in place: each newline becomes the end of one line. */
static int report(char *text, size_t max_rows) {
    char *line = text;
    char *nl = strchr(line, '\n');
    if (nl != NULL) *nl = '\0';
    const size_t ncpu = count_cpus(line);
    printf("  header: %u online CPUs\n", (unsigned)ncpu);
    Row r;
    r.cpu = malloc((ncpu > 0 ? ncpu : 1) * sizeof r.cpu[0]);
    if (r.cpu == NULL) {
        printf("  out of memory\n");
        return 1;
    }
    size_t kept = 0, dropped = 0;
    while (nl != NULL) {
        line = nl + 1;
        nl = strchr(line, '\n');
        if (nl != NULL) *nl = '\0';
        if (nl == NULL && *line == '\0') break;     /* the text ended with a newline */
        parse_row(line, ncpu, &r);
        if (r.n != ncpu) {                          /* the caller drops the short rows */
            printf("  dropped %s (%u of %u counters)\n", r.label, (unsigned)r.n, (unsigned)ncpu);
            ++dropped;
            continue;
        }
        ++kept;
        if (kept > max_rows) continue;
        uint64_t total = 0;
        size_t top = 0;
        for (size_t c = 0; c < r.n; ++c) {
            total += r.cpu[c];
            if (r.cpu[c] > r.cpu[top]) top = c;
        }
        printf("  %s total %" PRIu64 ", busiest CPU%u with %" PRIu64 "\n", r.label, total, (unsigned)top,
               ncpu > 0 ? r.cpu[top] : 0);
    }
    if (kept > max_rows) printf("  (%u more rows parsed, not printed)\n", (unsigned)(kept - max_rows));
    printf("  rows kept: %u, rows dropped: %u\n", (unsigned)kept, (unsigned)dropped);
    free(r.cpu);
    return 0;
}

/* The whole file as one malloc'd, null terminated text; NULL when it cannot be opened.
   /proc files report size 0, so read in chunks until the end. */
static char *read_all(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) return NULL;
    size_t cap = 4096, len = 0;
    char *text = malloc(cap);
    while (text != NULL) {
        size_t got = fread(text + len, 1, cap - len - 1, f);
        len += got;
        if (got == 0) break;
        if (cap - len - 1 == 0) {
            char *bigger = realloc(text, cap * 2);
            if (bigger == NULL) { free(text); text = NULL; break; }
            text = bigger;
            cap *= 2;
        }
    }
    fclose(f);
    if (text != NULL) text[len] = '\0';
    return text;
}

int main(void) {
    /* A typical file, so the parser shows its work on every operating system. */
    char typical[] =
        "           CPU0       CPU1       CPU2       CPU3\n"
        " 24:          0          0     918204          0  PCI-MSIX  eth0-rx-0\n"
        " 25:          0          0          0     877512  PCI-MSIX  eth0-rx-1\n"
        " 31:      40211          0          0          0  PCI-MSIX  nvme0q1\n"
        "LOC:     902113     899120     911822     915377  Local timer interrupts\n"
        "RES:      37226      39409      51080      35507  Rescheduling interrupts\n"
        "ERR:          0\n"
        "MIS:          0\n";
    printf("typical /proc/interrupts (built in):\n");
    if (report(typical, 16) != 0) return 1;

    /* The real file: a plain read, no root. It exists on Linux only (WSL included). */
    char *real = read_all("/proc/interrupts");
    if (real == NULL) {
        printf("this part needs Linux: it would parse the real /proc/interrupts of this machine\n");
        return 0;
    }
    printf("\n/proc/interrupts of this machine (counts since boot):\n");
    int rc = report(real, 12);
    free(real);
    return rc;
}
