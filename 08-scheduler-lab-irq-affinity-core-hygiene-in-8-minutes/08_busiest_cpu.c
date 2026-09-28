/* IRQ Affinity and Core Hygiene in 8 Minutes - slide 8: one second of deltas: the busiest cpu (C version of 08_busiest_cpu.cpp) */
/* Build: make 08_busiest_cpu_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static void sleep_one_second(void) { Sleep(1000); }
#else
#include <time.h>
static void sleep_one_second(void) {
    struct timespec ts = { 1, 0 };
    nanosleep(&ts, NULL);
}
#endif

/* C has no std::map: a Snapshot is an array of rows kept sorted by label, searched with bsearch.
   "25:" or "LOC:" -> one counter per CPU. */
typedef struct { char label[32]; uint64_t *cpu; } SRow;
typedef struct { SRow *rows; size_t n, cap, ncpu; } Snapshot;

/* Two typical files one second apart, used when /proc/interrupts does not exist (Windows, macOS). */
static const char *const TYPICAL[2] = {
    "           CPU0       CPU1       CPU2       CPU3\n"
    " 24:          0          0     918204          0  PCI-MSIX  eth0-rx-0\n"
    " 25:          0          0          0     877512  PCI-MSIX  eth0-rx-1\n"
    " 31:      40211          0          0          0  PCI-MSIX  nvme0q1\n"
    "LOC:     902113     899120     911822     915377  Local timer interrupts\n"
    "RES:      37226      39409      51080      35507  Rescheduling interrupts\n"
    "ERR:          0\n",
    "           CPU0       CPU1       CPU2       CPU3\n"
    " 24:          0          0     930516          0  PCI-MSIX  eth0-rx-0\n"
    " 25:          0          0          0     886040  PCI-MSIX  eth0-rx-1\n"
    " 31:      40563          0          0          0  PCI-MSIX  nvme0q1\n"
    "LOC:     902363     899370     912072     915627  Local timer interrupts\n"
    "RES:      37241      39420      51102      35519  Rescheduling interrupts\n"
    "ERR:          0\n"};

static bool g_builtin = false;                          /* true when the typical text was used */

/* The whole file as one malloc'd text: one open, chunked reads (/proc files report size 0).
   A copy of the built in text when the file is missing. NULL only when out of memory. */
static char *read_all(const char *path, int which) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        g_builtin = true;
        const char *src = TYPICAL[which % 2];
        char *copy = malloc(strlen(src) + 1);
        if (copy != NULL) memcpy(copy, src, strlen(src) + 1);
        return copy;
    }
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

/* Next whitespace separated word of *p: its start and length, 0 at the end of the line. */
static size_t next_word(const char **p, const char **start) {
    const char *s = *p;
    while (*s == ' ' || *s == '\t' || *s == '\r') ++s;
    *start = s;
    while (*s != '\0' && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') ++s;
    *p = s;
    return (size_t)(s - *start);
}

/* The header line is "CPU0 CPU1 CPU2": one word per online CPU. */
static size_t count_words(const char *line) {
    const char *p = line, *w;
    size_t n = 0;
    while (next_word(&p, &w) > 0) ++n;
    return n;
}

static size_t cpu_count(const char *path) {
    char *text = read_all(path, 0);
    if (text == NULL) return 0;
    char *nl = strchr(text, '\n');
    if (nl != NULL) *nl = '\0';
    size_t n = count_words(text);
    free(text);
    return n;
}

static int cmp_label(const void *a, const void *b) {
    return strcmp(((const SRow *)a)->label, ((const SRow *)b)->label);
}

static void snapshot_free(Snapshot *s) {        /* no destructor in C: every snapshot is freed by hand */
    for (size_t i = 0; i < s->n; ++i) free(s->rows[i].cpu);
    free(s->rows);
    s->rows = NULL;
    s->n = s->cap = 0;
}

/* The parser of slide 7, storing the rows by label. Short rows (ERR, MIS) are dropped.
   Returns false when out of memory. */
static bool snapshot(const char *path, Snapshot *out) {
    static int reads = 0;
    Snapshot s = { NULL, 0, 0, 0 };
    char *text = read_all(path, reads++);
    if (text == NULL) return false;
    char *line = text;
    char *nl = strchr(line, '\n');
    if (nl != NULL) *nl = '\0';
    s.ncpu = count_words(line);
    bool ok = true;
    while (nl != NULL && ok) {
        line = nl + 1;
        nl = strchr(line, '\n');
        if (nl != NULL) *nl = '\0';
        const char *p = line, *w;
        size_t len = next_word(&p, &w);
        if (len == 0 || s.ncpu == 0) continue;
        SRow r;
        if (len >= sizeof r.label) len = sizeof r.label - 1;
        memcpy(r.label, w, len);
        r.label[len] = '\0';
        r.cpu = malloc(s.ncpu * sizeof r.cpu[0]);
        if (r.cpu == NULL) { ok = false; break; }
        size_t n = 0;
        while (n < s.ncpu && next_word(&p, &w) > 0 && *w >= '0' && *w <= '9')
            r.cpu[n++] = strtoull(w, NULL, 10);
        if (n != s.ncpu) { free(r.cpu); continue; }
        SRow *dup = NULL;                       /* rows[label] = cpu: a repeated label replaces */
        for (size_t i = 0; i < s.n; ++i)
            if (strcmp(s.rows[i].label, r.label) == 0) dup = &s.rows[i];
        if (dup != NULL) { free(dup->cpu); dup->cpu = r.cpu; continue; }
        if (s.n == s.cap) {
            size_t cap = s.cap ? s.cap * 2 : 64;
            SRow *bigger = realloc(s.rows, cap * sizeof *bigger);
            if (bigger == NULL) { free(r.cpu); ok = false; break; }
            s.rows = bigger;
            s.cap = cap;
        }
        s.rows[s.n++] = r;
    }
    free(text);
    if (!ok) { snapshot_free(&s); return false; }
    if (s.n > 0) qsort(s.rows, s.n, sizeof s.rows[0], cmp_label);     /* map order: by label */
    *out = s;
    return true;
}

/* Numbered rows are device interrupts, the ones an affinity file can move. */
static bool is_device(const char *label) { return label[0] >= '0' && label[0] <= '9'; }

/* now minus before for one row into d; zeros when the row is new or changed its width (CPU hotplug). */
static void delta(const Snapshot *before, const SRow *now, size_t ncpu, uint64_t *d) {
    for (size_t c = 0; c < ncpu; ++c) d[c] = 0;
    const SRow *was = before->n ? bsearch(now, before->rows, before->n, sizeof before->rows[0], cmp_label) : NULL;
    if (was == NULL || before->ncpu != ncpu) return;
    for (size_t c = 0; c < ncpu; ++c)
        if (now->cpu[c] >= was->cpu[c]) d[c] = now->cpu[c] - was->cpu[c];
}

typedef struct { uint64_t count; const char *label; } Top;

/* largest count first, ties by the larger label: the order of std::sort on reverse iterators */
static int cmp_top_desc(const void *a, const void *b) {
    const Top *x = a, *y = b;
    if (x->count != y->count) return x->count < y->count ? 1 : -1;
    return -strcmp(x->label, y->label);
}

int main(void) {
    /* Also the warm up read: the first read of a /proc file can fault in pages and allocate. */
    const size_t ncpu = cpu_count("/proc/interrupts");
    if (ncpu == 0) {
        printf("no CPU columns found in /proc/interrupts: nothing to measure\n");
        return 0;
    }

    /* Two snapshots one second apart: the delta is the interrupt rate.
       Reading needs no root; only moving an IRQ does. */
    Snapshot before, after;
    if (!snapshot("/proc/interrupts", &before)) { printf("out of memory\n"); return 1; }
    sleep_one_second();
    if (!snapshot("/proc/interrupts", &after)) { snapshot_free(&before); printf("out of memory\n"); return 1; }

    int rc = 0;
    uint64_t *device = calloc(ncpu, sizeof *device);
    uint64_t *local = calloc(ncpu, sizeof *local);
    uint64_t *d = calloc(after.ncpu ? after.ncpu : 1, sizeof *d);      /* one delta per column of the row */
    Top *top = calloc(after.n ? after.n : 1, sizeof *top);
    if (device == NULL || local == NULL || d == NULL || top == NULL) {
        printf("out of memory\n");
        rc = 1;
        goto done;
    }
    const size_t width = after.ncpu < ncpu ? after.ncpu : ncpu;   /* columns summed (CPU hotplug safe) */
    for (size_t i = 0; i < after.n; ++i) {
        uint64_t *sum = is_device(after.rows[i].label) ? device : local;  /* "25:" or "LOC:" */
        delta(&before, &after.rows[i], after.ncpu, d);                    /* zeros if new */
        for (size_t c = 0; c < width; ++c) sum[c] += d[c];
    }
    size_t busiest = 0, timer = 0;
    for (size_t c = 0; c < ncpu; ++c) {
        if (device[c] > device[busiest]) busiest = c;
        if (local[c] > local[timer]) timer = c;
    }

    printf(g_builtin ? "source: built in typical snapshots (this sample needs Linux to read the real "
                       "/proc/interrupts)\n"
                     : "source: /proc/interrupts of this machine\n");
    printf("interrupts in one second, %u online CPUs, %u rows:\n", (unsigned)ncpu, (unsigned)after.n);
    printf("  CPU     device      local\n");
    for (size_t c = 0; c < ncpu; ++c)
        printf("  %3u%11" PRIu64 "%11" PRIu64 "\n", (unsigned)c, device[c], local[c]);

    if (device[busiest] == 0) {
        printf("no device interrupt in this second: run it again during network or disk work\n");
    } else {
        printf("busiest CPU for device interrupts: CPU%u with %" PRIu64 " in one second\n", (unsigned)busiest,
               device[busiest]);
    }
    printf("busiest CPU for local interrupts (timer, IPIs): CPU%u with %" PRIu64 ", no affinity file moves these\n",
           (unsigned)timer, local[timer]);

    /* The three sources that fired most: the rows to move first. */
    for (size_t i = 0; i < after.n; ++i) {
        delta(&before, &after.rows[i], after.ncpu, d);
        uint64_t total = 0;
        for (size_t c = 0; c < after.ncpu; ++c) total += d[c];
        top[i].count = total;
        top[i].label = after.rows[i].label;
    }
    if (after.n > 0) qsort(top, after.n, sizeof top[0], cmp_top_desc);
    printf("top sources:");
    for (size_t i = 0; i < after.n && i < 3; ++i) printf("  %s %" PRIu64, top[i].label, top[i].count);
    printf("\nhygiene check: after moving the IRQs, the device column of your application cores reads 0\n");

done:                                           /* C has no destructor: one cleanup path frees everything */
    free(top);
    free(d);
    free(local);
    free(device);
    snapshot_free(&after);
    snapshot_free(&before);
    return rc;
}
