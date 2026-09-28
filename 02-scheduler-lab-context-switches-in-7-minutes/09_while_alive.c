/* Context Switches in 7 Minutes: The Hidden Cost - slide 9: thank you (C version of 09_while_alive.cpp) */
/* Build: make 09_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>

/* The imaginary tomjnet.h of the slide. */
typedef struct { char name[64]; } Topic;
static int episodes = 0;
static int alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_scheduler_topic(void) {
    static const char *topics[] = {"CPU migration and cache warmth", "pinning with taskset",
                                   "CPU affinity in code: sched_setaffinity"};
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

/* C has no std::queue: a fixed array with a head and a tail index is the FIFO */
#define QUEUE_CAP 8
typedef struct { Topic items[QUEUE_CAP]; int head; int tail; } TopicQueue;
static int queue_push(TopicQueue *q, Topic t) {
    if (q->tail == QUEUE_CAP) return 0;
    q->items[q->tail++] = t;
    return 1;
}

int main(void) {
    static TopicQueue run_queue;
    while (alive()) {
        if (!queue_push(&run_queue, next_scheduler_topic())) return 1;  /* CPU migration */
        if (!queue_push(&run_queue, viewer_request())) return 1;        /* wakes up next */
        subscribe();                                                    /* never preempted */
    }

    printf("run queue, in the order it will be scheduled:\n");
    while (run_queue.head != run_queue.tail) {
        printf("  %s\n", run_queue.items[run_queue.head].name);
        ++run_queue.head;
    }
    return 0;
}
