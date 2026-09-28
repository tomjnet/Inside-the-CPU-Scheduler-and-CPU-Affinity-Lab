// A Repeatable Low Latency Core Recipe in 10 Minutes - slide 7: stage 5: priority, with a safety net
// Build: make 07_priority
#include <cstdio>

#if defined(__linux__)
#include <sched.h>
#include <time.h>

static long long now_ns() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1'000'000'000LL + t.tv_nsec;
}

static int g_polls = 0;
static long long g_worst_ns = 0;
static bool done() { return g_polls >= 500; }          // the work ends on its own, well before the timeout

// One unit of latency sensitive work: sleep 1 ms, record how late the wakeup was.
static void poll_once() {
    timespec nap{};
    nap.tv_nsec = 1'000'000;
    long long t0 = now_ns();
    clock_nanosleep(CLOCK_MONOTONIC, 0, &nap, nullptr);
    long long late = now_ns() - t0 - 1'000'000;
    if (late > g_worst_ns) g_worst_ns = late;
    ++g_polls;
}

static const char* policy_name(int policy) {
    switch (policy) {
        case SCHED_FIFO: return "SCHED_FIFO";
        case SCHED_RR: return "SCHED_RR";
        case SCHED_OTHER: return "SCHED_OTHER";
        default: return "another policy";
    }
}
#endif

int main() {
#if defined(__linux__)
    std::printf("policy at start: %s\n", policy_name(sched_getscheduler(0)));

    // Stage 5: real time priority, only with a safety net
    sched_param sp{};
    sp.sched_priority = 50;                    // 1 to 99: 50 is plenty
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
        std::perror("sched_setscheduler");     // EPERM without root
        std::puts("continuing as SCHED_OTHER");
    }
    std::printf("policy in the loop: %s\n", policy_name(sched_getscheduler(0)));
    long long stop = now_ns() + 2'000'000'000; // the timeout: 2 seconds
    while (now_ns() < stop && !done())         // never an endless FIFO loop
        poll_once();
    sp.sched_priority = 0;
    sched_setscheduler(0, SCHED_OTHER, &sp);   // rollback in the same file

    std::printf("%d wakeups, worst overshoot %lld us on this machine\n", g_polls, g_worst_ns / 1000);
    std::printf("policy at exit: %s\n", policy_name(sched_getscheduler(0)));
    std::printf("with root (sudo, or CAP_SYS_NICE) the loop runs as SCHED_FIFO 50; the timeout stays either way\n");
#else
    std::printf("this sample needs Linux: it asks for SCHED_FIFO priority 50 with sched_setscheduler, reports EPERM\n"
                "without root, runs a loop bounded by a 2 second timeout and returns to SCHED_OTHER\n");
#endif
    return 0;
}
