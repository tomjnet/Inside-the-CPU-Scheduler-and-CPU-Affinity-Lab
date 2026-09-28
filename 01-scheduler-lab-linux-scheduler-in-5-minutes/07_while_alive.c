/* The Linux Scheduler in 5 Minutes: Who Runs Next? - slide 7: thank you (C version of 07_while_alive.cpp) */
/* Build: make 07_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* the imaginary tomjnet.h of the slide */
/* C has no std::string: the name lives inside the object */
typedef struct { char name[64]; } Topic;
static int episodes = 0;
static bool alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_lab_topic(void) {
    static const char *topics[] = {"context switches and their hidden cost", "CPU migration and cache warmth",
                                   "pinning with taskset"};
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
    TopicQueue run_queue = {0};                /* one per CPU */
    while (alive()) {
        push(&run_queue, next_lab_topic());    /* context switches */
        push(&run_queue, viewer_request());    /* runnable, waiting */
        subscribe();                           /* never preempted */
    }

    printf("runnable topics: %zu\n", run_queue.count);
    while (run_queue.count != 0) {             /* first in, first out: the scheduler picks the front */
        const Topic t = pop(&run_queue);
        printf("  running: %s\n", t.name);
    }
    return 0;
}
