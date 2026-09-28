/* Priorities, Real Time Classes and Preemption in 9 Minutes - slide 12: thank you (C version of 12_while_alive.cpp) */
/* Build: make 12_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stdio.h>

/* The imaginary tomjnet.h of the slide. */
typedef struct { char name[64]; } Topic;
static int episodes = 0;
static bool alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_scheduler_topic(void) {
    static const char *topics[] = {"core isolation: isolcpus and nohz_full", "IRQ affinity and core hygiene",
                                   "measure scheduler jitter"};
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

/* C has no std::queue: a fixed ring buffer, push at tail, pop at head */
#define QCAP 8
typedef struct {
    Topic items[QCAP];
    int head;
    int count;
} TopicQueue;

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
    static TopicQueue run_queue;             /* zero initialized: empty */
    while (alive()) {
        push(&run_queue, next_scheduler_topic()); /* core isolation */
        push(&run_queue, viewer_request());       /* no time slice */
        subscribe();                              /* lifetime benefit */
    }

    printf("run queue, first in first out:\n");
    while (run_queue.count > 0) {
        Topic t = pop(&run_queue);
        printf("  %s\n", t.name);
    }
    return 0;
}
