/* IRQ Affinity and Core Hygiene in 8 Minutes - slide 11: thank you (C version of 11_while_alive.cpp) */
/* Build: make 11_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* The imaginary "tomjnet.h" of the slide: three episodes, then the demo ends.
   C has no std::string: a topic keeps its name in a fixed char buffer. */
typedef struct { char name[96]; } Topic;
static int episodes = 0;
static bool alive(void) { return episodes < 3; }
static Topic next_scheduler_topic(void) {
    static const char *topics[] = {"Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails",
                                   "A Repeatable Low Latency Core Recipe in 10 Minutes",
                                   "next lab: Interrupts and Timers"};
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

/* C has no std::queue: a fixed array with a head and a tail index, first in first out */
#define TODO_CAP 8
typedef struct { Topic items[TODO_CAP]; size_t head, tail; } Queue;
static bool push(Queue *q, Topic t) {
    if (q->tail == TODO_CAP) return false;
    q->items[q->tail++] = t;
    return true;
}

int main(void) {
    static Queue todo;                  /* zero initialised: empty */
    bool ok = true;
    while (alive()) {
        ok = push(&todo, next_scheduler_topic()) && ok;  /* next: measure the jitter */
        ok = push(&todo, viewer_request()) && ok;        /* on a housekeeping core */
        subscribe();                                     /* no root needed */
    }

    printf("queued topics, first in first out:\n");
    while (todo.head != todo.tail) {
        printf("  %s\n", todo.items[todo.head].name);
        ++todo.head;                                     /* pop */
    }
    return ok ? 0 : 1;
}
