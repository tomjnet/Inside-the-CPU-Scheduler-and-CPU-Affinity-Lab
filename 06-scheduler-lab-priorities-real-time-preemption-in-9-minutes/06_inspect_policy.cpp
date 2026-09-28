// Priorities, Real Time Classes and Preemption in 9 Minutes - slide 6: inspect the scheduling policy
// Build: make 06_inspect_policy
#include <iostream>

#if defined(__linux__)
#include <sched.h>
#include <time.h>

static const char* name_of(int policy) {
    switch (policy) {
        case SCHED_OTHER: return "SCHED_OTHER";
        case SCHED_FIFO: return "SCHED_FIFO";
        case SCHED_RR: return "SCHED_RR";
        case SCHED_BATCH: return "SCHED_BATCH";
        case SCHED_IDLE: return "SCHED_IDLE";
        default: return "another policy (SCHED_DEADLINE is 6)";
    }
}
#endif

int main() {
    std::cout << "policies: SCHED_OTHER, SCHED_BATCH, SCHED_IDLE (fair class, nice), "
                 "SCHED_FIFO, SCHED_RR (real time, 1..99), SCHED_DEADLINE\n";
#if defined(__linux__)
    // Which policy am I? One system call each, no root needed.
    int policy = sched_getscheduler(0);           // 0 = calling thread
    sched_param sp{};
    sched_getparam(0, &sp);                       // 0 in the fair class
    int lo = sched_get_priority_min(SCHED_FIFO);  // 1 on Linux
    int hi = sched_get_priority_max(SCHED_FIFO);  // 99 on Linux
    timespec slice{};
    sched_rr_get_interval(0, &slice);             // the RR time slice
    std::cout << name_of(policy) << ", priority " << sp.sched_priority
              << ", FIFO range " << lo << ".." << hi << '\n';

    std::cout << "SCHED_RR range " << sched_get_priority_min(SCHED_RR) << ".." << sched_get_priority_max(SCHED_RR)
              << ", SCHED_OTHER range " << sched_get_priority_min(SCHED_OTHER) << ".."
              << sched_get_priority_max(SCHED_OTHER) << " (nice is the knob there)\n";
    std::cout << "sched_rr_get_interval: " << slice.tv_sec * 1000 + slice.tv_nsec / 1000000
              << " ms (the round robin slice under SCHED_RR, the fair slice otherwise)\n";
    std::cout << "try as root: chrt -r 20 ./06_inspect_policy, and watch the first line change\n";
#else
    std::cout << "this sample needs Linux: sched_getscheduler would show SCHED_OTHER, priority 0, FIFO range 1..99\n";
#endif
    return 0;
}
