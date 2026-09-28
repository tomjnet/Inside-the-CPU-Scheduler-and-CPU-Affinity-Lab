// Priorities, Real Time Classes and Preemption in 9 Minutes - slide 7: ask for sched_fifo and handle eperm
// Build: make 07_set_fifo
#include <iostream>

#if defined(__linux__)
#include <cerrno>
#include <cstring>
#include <sched.h>

// SCHED_FIFO needs root, CAP_SYS_NICE or RLIMIT_RTPRIO: else EPERM.
bool go_real_time(int prio) {                  // 1..98, never 99
    sched_param sp{}; sp.sched_priority = prio;
    if (sched_setscheduler(0, SCHED_FIFO, &sp) == 0) return true;
    std::cout << "refused: " << std::strerror(errno)
              << ", staying SCHED_OTHER\n";    // report and continue
    return false;
}
void back_to_normal() {
    sched_param sp{};                          // priority 0: fair
    sched_setscheduler(0, SCHED_OTHER, &sp);   // always undo
}

static void show(const char* when) {
    sched_param sp{};
    sched_getparam(0, &sp);
    int policy = sched_getscheduler(0);
    const char* name = policy == SCHED_FIFO ? "SCHED_FIFO" : policy == SCHED_OTHER ? "SCHED_OTHER" : "another policy";
    std::cout << when << ": " << name << ", priority " << sp.sched_priority << '\n';
}
#endif

int main() {
    std::cout << "asking for SCHED_FIFO priority 50 (the middle: 99 belongs to kernel threads)\n";
#if defined(__linux__)
    show("before");
    bool granted = go_real_time(50);
    show("after the request");
    if (granted)
        std::cout << "granted: you are root, or have CAP_SYS_NICE, or ulimit -r allows 50\n";
    else
        std::cout << "with sudo that line would read SCHED_FIFO, priority 50; the program continues either way\n";
    back_to_normal();
    show("after back_to_normal");
#else
    std::cout << "this sample needs Linux: sched_setscheduler would report EPERM without root, then continue\n";
#endif
    return 0;
}
