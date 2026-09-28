// Pinning with taskset in 6 Minutes: One Process, One Core - slide 6: a program that prints its allowed cpus
// Build: make 06_allowed_cpus
// Try:   ./06_allowed_cpus    then    taskset -c 2 ./06_allowed_cpus    then    taskset 0xd ./06_allowed_cpus
#include <iostream>
#include <thread>

#if defined(__linux__)
#include <cstdio>
#include <sched.h>
#endif

int main() {
    std::cout << "logical CPUs the C++ library reports: " << std::thread::hardware_concurrency() << '\n';

#if defined(__linux__)
    // Ask the kernel: which CPUs may this process run on? No root needed
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) != 0) {   // 0 = myself
        std::perror("sched_getaffinity");
        return 1;
    }
    std::cout << "allowed: " << CPU_COUNT(&set) << " CPUs:";
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
        if (CPU_ISSET(cpu, &set)) std::cout << ' ' << cpu;
    std::cout << "\nrunning on CPU " << sched_getcpu() << '\n';

    // the same set as the hex mask taskset prints with -p (first 64 CPUs)
    unsigned long long mask = 0;
    for (int cpu = 0; cpu < 64; ++cpu)
        if (CPU_ISSET(cpu, &set)) mask |= 1ull << cpu;
    std::cout << "as a hex mask: 0x" << std::hex << mask << std::dec << '\n';

    if (CPU_COUNT(&set) == 1)
        std::cout << "pinned: one allowed CPU, the load balancer cannot migrate this process\n";
    else
        std::cout << "not pinned: run me again under  taskset -c 2  and compare\n";
#else
    std::cout << "this sample needs Linux: it would print the CPUs the process is allowed on "
                 "(sched_getaffinity) and the CPU it runs on (sched_getcpu), all of them when run plain, "
                 "exactly one under taskset -c 2\n";
#endif
    return 0;
}
