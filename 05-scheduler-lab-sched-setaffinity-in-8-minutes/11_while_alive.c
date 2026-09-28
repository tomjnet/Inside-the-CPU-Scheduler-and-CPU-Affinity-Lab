/* CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 11: thank you (C version of 11_while_alive.cpp) */
/* Build: make 11_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stdio.h>

/* the imaginary tomjnet.h of the slide */
typedef struct { char name[64]; } Topic;
static int episodes = 0;
static bool alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_scheduler_topic(void) {
    static const char *topics[] = {"Priorities, Real Time Classes and Preemption",
                                   "Core Isolation: isolcpus and nohz_full",
                                   "IRQ Affinity and Core Hygiene"};
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

/* C has no std::queue: a fixed array with a head and a tail index */
#define QUEUE_CAP 16
typedef struct { Topic items[QUEUE_CAP]; int head; int tail; } TopicQueue;
static void push(TopicQueue *q, Topic t) { if (q->tail < QUEUE_CAP) q->items[q->tail++] = t; }
static bool empty(const TopicQueue *q) { return q->head == q->tail; }

int main(void) {
    static TopicQueue run_queue;             /* static: zero head and tail */
    while (alive()) {
        push(&run_queue, next_scheduler_topic());   /* priorities are next */
        push(&run_queue, viewer_request());         /* never starved */
        subscribe();                                /* pinned to TomJNet */
    }

    printf("run queue of the Scheduler and CPU Affinity Lab:\n");
    while (!empty(&run_queue)) {
        printf("  %s\n", run_queue.items[run_queue.head].name);
        ++run_queue.head;
    }
    return 0;
}
