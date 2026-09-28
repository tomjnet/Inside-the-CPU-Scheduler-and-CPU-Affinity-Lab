// CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 3: cpu_set_t: one bit per cpu
// Build: make 03_cpu_set
#include <cstdio>
#include <iostream>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <sched.h>
#endif

int main() {
    // portable: the number of logical CPUs of the machine, not the number this thread may use
    std::cout << "hardware_concurrency: " << std::thread::hardware_concurrency() << " logical CPUs\n";

#if defined(__linux__)
    // cpu_set_t: a fixed bitmap, one bit per logical CPU (1024 bits)
    cpu_set_t allowed;
    CPU_ZERO(&allowed);                          // macro: no system call
    // ask the kernel where this thread may run: one system call
    if (sched_getaffinity(0, sizeof allowed, &allowed) != 0)
        perror("sched_getaffinity");

    std::vector<int> cpus;                       // the allowed CPU numbers
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
        if (CPU_ISSET(cpu, &allowed)) cpus.push_back(cpu);

    printf("%d CPUs allowed\n", CPU_COUNT(&allowed));

    std::cout << "sizeof(cpu_set_t): " << sizeof(cpu_set_t) << " bytes = " << CPU_SETSIZE << " bits\n";
    std::cout << "allowed list:";
    for (int cpu : cpus) std::cout << ' ' << cpu;
    std::cout << "\n";

    // the macros are plain bit operations on the set: build one by hand and test it
    cpu_set_t mine;
    CPU_ZERO(&mine);
    if (!cpus.empty()) CPU_SET(cpus.front(), &mine);
    if (cpus.size() > 1) CPU_SET(cpus.back(), &mine);
    std::cout << "hand built set: " << CPU_COUNT(&mine) << " bits on, first allowed CPU is "
              << (!cpus.empty() && CPU_ISSET(cpus.front(), &mine) ? "set" : "clear") << "\n";
    std::cout << "try: taskset -c 0,1 ./03_cpu_set   (the allowed list shrinks to 0 1)\n";
#else
    std::cout << "this sample needs Linux: the allowed CPU list of this thread from sched_getaffinity\n";
#endif
    return 0;
}
