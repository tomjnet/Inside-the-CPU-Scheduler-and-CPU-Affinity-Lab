/* CPU Migration and Cache Warmth in 7 Minutes - slide 9: thank you (C version of 09_while_alive.cpp) */
/* Build: make 09_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* snprintf and friends: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>

/* the imaginary tomjnet.h of the slide, written out so the program builds */
typedef struct { char name[64]; } Topic;
static int episodes = 0;
static int alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_scheduler_topic(void) {
    static const char *topics[] = {"Pinning with taskset: one process, one core",
                                   "CPU affinity in code: sched_setaffinity",
                                   "Priorities, real time classes and preemption"};
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

/* C has no std::queue: a fixed ring buffer with a head, a count and a capacity check */
#define QUEUE_CAP 8
typedef struct { Topic items[QUEUE_CAP]; size_t head; size_t count; } TopicQueue;
static int queue_push(TopicQueue *q, Topic t) {
    if (q->count == QUEUE_CAP) return 0;
    q->items[(q->head + q->count) % QUEUE_CAP] = t;
    ++q->count;
    return 1;
}
static Topic queue_pop(TopicQueue *q) {
    Topic t = q->items[q->head];
    q->head = (q->head + 1) % QUEUE_CAP;
    --q->count;
    return t;
}

int main(void) {
    static TopicQueue run_queue;
    while (alive()) {
        int ok = queue_push(&run_queue, next_scheduler_topic());  /* next: taskset */
        ok &= queue_push(&run_queue, viewer_request());           /* wakes up first */
        if (!ok) { printf("run queue full\n"); return 1; }
        subscribe();                                              /* never migrates */
    }

    printf("run queue of the Scheduler and CPU Affinity Lab, %zu entries:\n", run_queue.count);
    while (run_queue.count > 0) {
        Topic t = queue_pop(&run_queue);
        printf("  %s\n", t.name);
    }
    return 0;
}
