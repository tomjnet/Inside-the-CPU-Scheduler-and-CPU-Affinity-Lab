/* Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 12: thank you (C version of 12_while_alive.cpp) */
/* Build: make 12_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>

/* the imaginary tomjnet.h of the slide */
typedef struct { const char *name; } Topic;
static int episodes = 0;
static int alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_scheduler_topic(void) {
    static const char *topics[] = {"A Repeatable Low Latency Core Recipe in 10 Minutes",
                                   "Interrupts and Timers Lab: the IRQ path",
                                   "Low Latency C++ Lab: the whole loop, measured"};
    Topic t = {topics[episodes++]};
    return t;
}
typedef struct {
    Topic seen[3];                   /* C has no growing vector: a fixed array and a count */
    size_t count;
} Histogram;
static void record(Histogram *h, Topic t) {
    if (h->count < sizeof h->seen / sizeof h->seen[0]) h->seen[h->count++] = t;
    printf("episode %zu: %s\n", h->count, t.name);
}
static void measure_again(void) { printf("  measured again: p50, p99, p99.9, max\n"); }
static void subscribe(void) { printf("  subscribed to TomJNet\n"); }

int main(void) {
    Histogram channel = {0};         /* p50, p99, p99.9, max */
    while (alive()) {
        record(&channel, next_scheduler_topic());   /* the recipe is next */
        measure_again();                 /* one change at a time */
        subscribe();                     /* zero jitter, guaranteed */
    }
    printf("%zu topics recorded\n", channel.count);
    return 0;
}
