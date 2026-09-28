// Priorities, Real Time Classes and Preemption in 9 Minutes - slide 3: nice values: a weight, not a guarantee
// Build: make 03_nice
#include <cmath>
#include <iomanip>
#include <iostream>

#if defined(__linux__)
#include <sys/resource.h>
#endif

// nice: -20 (greedy) to 19 (kind), a weight inside the fair class.
// One step is about 1.25x: nice 0 = 1024, nice 19 = about 15.
double weight(int nice) { return 1024.0 / std::pow(1.25, nice); }
double share(int a, int b) {            // two busy tasks, one core
    return weight(a) / (weight(a) + weight(b));
}

#if defined(__linux__)
void be_kind() {
    int before = getpriority(PRIO_PROCESS, 0);   // 0 = this thread
    setpriority(PRIO_PROCESS, 0, before + 10);   // kinder: no root
    // going back down needs CAP_SYS_NICE: one system call each
}
#endif

int main() {
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "the weight model (the kernel table is close to it, rounded by hand):\n";
    for (int nice : {-20, -10, -5, 0, 5, 10, 19})
        std::cout << "  nice " << std::setw(3) << nice << "  weight " << std::setw(8) << weight(nice) << '\n';

    std::cout << "share of one core, two busy tasks:\n";
    const int pairs[][2] = {{0, 0}, {0, 1}, {0, 5}, {0, 10}, {0, 19}, {-20, 19}};
    for (const auto& p : pairs)
        std::cout << "  nice " << std::setw(3) << p[0] << " against nice " << std::setw(2) << p[1] << ":  "
                  << std::setw(5) << 100.0 * share(p[0], p[1]) << " % against "
                  << std::setw(5) << 100.0 * share(p[1], p[0]) << " %\n";

#if defined(__linux__)
    int was = getpriority(PRIO_PROCESS, 0);
    be_kind();
    int now = getpriority(PRIO_PROCESS, 0);
    std::cout << "this thread: nice " << was << " before be_kind(), nice " << now << " after\n";
    if (setpriority(PRIO_PROCESS, 0, was) != 0)
        std::cout << "going back to nice " << was << " was refused: that needs CAP_SYS_NICE or RLIMIT_NICE\n";
    else
        std::cout << "going back to nice " << was << " was allowed: you are root or RLIMIT_NICE permits it\n";
#else
    std::cout << "this sample needs Linux: getpriority and setpriority would show this thread go from nice 0 to 10\n";
#endif
    std::cout << "a share of the CPU, not a latency guarantee\n";
    return 0;
}
