/* A Repeatable Low Latency Core Recipe in 10 Minutes - slide 13: thank you (C version of 13_while_alive.cpp) */
/* Build: make 13_while_alive_c */
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
static bool alive(void) { return episodes < 3; }            /* three episodes, then the demo ends */
static Topic next_lab_topic(void) {
    static const char *topics[] = {"Interrupts and Timers Lab: the IRQ path", "Interrupts and Timers Lab: APIC routing",
                                   "Interrupts and Timers Lab: the kernel tick"};
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

/* C has no std::queue: a fixed ring of Topic with a head and a count */
#define QCAP 8
typedef struct { Topic items[QCAP]; int head; int count; } TopicQueue;
static bool q_push(TopicQueue *q, Topic t) {
    if (q->count == QCAP) return false;
    q->items[(q->head + q->count) % QCAP] = t;
    ++q->count;
    return true;
}
static Topic q_pop(TopicQueue *q) {
    Topic t = q->items[q->head];
    q->head = (q->head + 1) % QCAP;
    --q->count;
    return t;
}

int main(void) {
    TopicQueue todo = {0};
    while (alive()) {
        if (!q_push(&todo, next_lab_topic())) return 1;   /* Interrupts and Timers Lab */
        if (!q_push(&todo, viewer_request())) return 1;   /* FIFO: asked first, made first */
        subscribe();                                      /* lifetime benefit */
    }

    printf("next on the channel, in FIFO order:\n");
    while (todo.count > 0) {
        Topic t = q_pop(&todo);
        printf("  %s\n", t.name);
    }
    return 0;
}
