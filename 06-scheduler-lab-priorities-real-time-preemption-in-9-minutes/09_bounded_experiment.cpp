// Priorities, Real Time Classes and Preemption in 9 Minutes - slide 9: a bounded priority experiment
// Build: make 09_bounded_experiment
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>

#if defined(__linux__)
#include <cerrno>
#include <cstring>
#include <sched.h>
#include <sys/resource.h>
#include <thread>
#endif

// The weight model of slide 3: what the fair class should give two busy tasks on one core.
static double weight(int nice) { return 1024.0 / std::pow(1.25, nice); }
static double share(int a, int b) { return weight(a) / (weight(a) + weight(b)); }

#if defined(__linux__)
using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;

// Slide 7: ask, report EPERM, continue.
static bool go_real_time(int prio) {
    sched_param sp{};
    sp.sched_priority = prio;
    if (sched_setscheduler(0, SCHED_FIFO, &sp) == 0) return true;
    std::cout << "  SCHED_FIFO refused: " << std::strerror(errno) << ", staying SCHED_OTHER\n";
    return false;
}
static void back_to_normal() {
    sched_param sp{};
    sched_setscheduler(0, SCHED_OTHER, &sp);
}

// Episode 5: pin the calling thread to one CPU.
static void pin_to(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        std::cout << "  pinning to CPU " << cpu << " refused: " << std::strerror(errno) << '\n';
}

static int first_allowed_cpu() {
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) != 0) return 0;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
        if (CPU_ISSET(cpu, &set)) return cpu;
    return 0;
}

// Two spinners share ONE core until `stop`: who gets the cycles?
std::uint64_t spin(int cpu, int nice_value, bool fifo, Deadline stop) {
    pin_to(cpu);                               // episode 5
    setpriority(PRIO_PROCESS, 0, nice_value);  // per thread on Linux
    if (fifo) go_real_time(50);                // EPERM: stays fair
    std::uint64_t n = 0;
    while (Clock::now() < stop) ++n;           // the safety timeout
    back_to_normal();                          // always undo
    return n;                                  // count = CPU share
}
// fair threads trade context switches; a FIFO thread takes no turns

static void round_of(const char* title, int cpu, int nice_a, int nice_b, bool fifo_a) {
    std::cout << title << '\n';
    Deadline stop = Clock::now() + std::chrono::milliseconds(500);   // bounded: both threads stop here
    std::uint64_t a = 0;
    std::uint64_t b = 0;
    std::thread tb([&] { b = spin(cpu, nice_b, false, stop); });     // the fair one starts first
    std::thread ta([&] { a = spin(cpu, nice_a, fifo_a, stop); });
    ta.join();
    tb.join();
    double total = static_cast<double>(a + b);
    if (total <= 0.0) total = 1.0;
    std::cout << "  thread A: " << std::setw(12) << a << " iterations, " << std::setw(5)
              << 100.0 * static_cast<double>(a) / total << " %\n";
    std::cout << "  thread B: " << std::setw(12) << b << " iterations, " << std::setw(5)
              << 100.0 * static_cast<double>(b) / total << " %   (this machine)\n";
}
#endif

int main() {
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "weight model: nice 0 against nice 0 gives " << 100.0 * share(0, 0) << " %, nice 0 against nice 19 gives "
              << 100.0 * share(0, 19) << " % against " << 100.0 * share(19, 0) << " %\n";
#if defined(__linux__)
    int cpu = first_allowed_cpu();
    std::cout << "two threads pinned to CPU " << cpu << ", 500 ms per round, the deadline is inside the loop\n";
    round_of("round 1: A nice 0, B nice 0 (expect about half each)", cpu, 0, 0, false);
    round_of("round 2: A nice 0, B nice 19 (expect B to get a percent or two)", cpu, 0, 19, false);
    round_of("round 3: A asks for SCHED_FIFO 50, B fair nice 0", cpu, 0, 0, true);
    std::cout << "round 3 with root: A takes about 95 %, B lives on the throttling reserve;\n"
                 "without root: EPERM above and an even split. Take numbers on real Linux hardware.\n";
#else
    std::cout << "this sample needs Linux: two threads pinned to one core would show the split for "
                 "nice 0 against nice 19 and for SCHED_FIFO against fair\n";
#endif
    return 0;
}
