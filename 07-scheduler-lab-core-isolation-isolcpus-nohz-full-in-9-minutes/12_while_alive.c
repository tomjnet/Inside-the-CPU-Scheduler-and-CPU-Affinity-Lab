/* Core Isolation in 9 Minutes: isolcpus and nohz_full - slide 12: thank you (C version of 12_while_alive.cpp) */
/* Build: make 12_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* The imaginary tomjnet.h of the slide. */
/* C has no std::string: the name lives inside the object */
typedef struct { char name[64]; } Topic;
static int episodes = 0;
static bool alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_lab_topic(void) {
    static const char *topics[] = {"IRQ affinity and core hygiene", "measure scheduler jitter",
                                   "a repeatable low latency core recipe"};
    Topic t;
    snprintf(t.name, sizeof t.name, "%s", topics[episodes++]);
    return t;
}
static Topic viewer_request(void) {
    Topic t;
    snprintf(t.name, sizeof t.name, "viewer request #%d", episodes);
    return t;
}
static void subscribe(void) { printf("  subscribed to TomJNet\n"); }

/* C has no std::queue: a fixed ring buffer, push at the tail, pop at the head */
#define QCAP 8
typedef struct { Topic items[QCAP]; size_t head, count; } TopicQueue;
static bool push(TopicQueue *q, Topic t) {
    if (q->count == QCAP) return false;
    q->items[(q->head + q->count) % QCAP] = t;
    ++q->count;
    return true;
}
static Topic pop(TopicQueue *q) {
    Topic t = q->items[q->head];
    q->head = (q->head + 1) % QCAP;
    --q->count;
    return t;
}

int main(void) {
    TopicQueue lab = {0};
    while (alive()) {
        push(&lab, next_lab_topic());    /* IRQ affinity is next */
        push(&lab, viewer_request());    /* housekeeping CPU work */
        subscribe();                     /* lifetime benefit */
    }

    printf("next in the Scheduler and CPU Affinity Lab:\n");
    while (lab.count != 0) {
        const Topic t = pop(&lab);
        printf("  %s\n", t.name);
    }
    return 0;
}
