// Core Isolation in 9 Minutes: isolcpus and nohz_full - slide 9: run on the isolated core
// Build: make 09_pin_isolated
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#if defined(__linux__)
#include <sched.h>
#endif

static const char* kIsolated = "/sys/devices/system/cpu/isolated";

// First line of a small text file, empty when the file is missing.
static std::string read_line(const char* path) {
    std::ifstream in(path);
    std::string line;
    if (in) std::getline(in, line);
    return line;
}

// First CPU of a kernel CPU list ("2-3,6" gives 2), -1 when the list is empty.
static int first_cpu(const std::string& list) {
    if (list.empty() || list[0] < '0' || list[0] > '9') return -1;
    int cpu = 0;
    for (char ch : list) {
        if (ch < '0' || ch > '9' || cpu > 100000) break;
        cpu = cpu * 10 + (ch - '0');
    }
    return cpu;
}

#if defined(__linux__)
// Highest CPU this thread may use (a container or cgroup can allow fewer than the machine has).
static int last_allowed_cpu() {
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) return 0;
    int last = 0;
    for (int c = 0; c < CPU_SETSIZE; ++c)
        if (CPU_ISSET(c, &allowed)) last = c;
    return last;
}
#endif

int main() {
    std::cout << "first_cpu(\"2-3,6\") = " << first_cpu("2-3,6") << ", first_cpu(\"\") = " << first_cpu("")
              << "\n";
#if defined(__linux__)
    std::cout << "started on CPU " << sched_getcpu() << ", isolated list: \"" << read_line(kIsolated)
              << "\"\n";

    // isolcpus keeps everyone out, this program included: ask for the core.
    int cpu = first_cpu(read_line(kIsolated));   // -1: nothing isolated
    if (cpu < 0) cpu = last_allowed_cpu();       // fallback for the demo
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);                          // exactly one CPU
    // One system call and one migration, paid once at startup.
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        std::perror("sched_setaffinity");        // EINVAL: CPU not allowed
    std::cout << "asked for CPU " << cpu
              << ", running on CPU " << sched_getcpu() << "\n";

    if (first_cpu(read_line(kIsolated)) < 0)
        std::cout << "nothing is isolated here: on the lab machine with isolcpus=2-3 this asks for CPU 2\n";
#else
    std::cout << "this sample needs Linux: it would pin itself to the first CPU of " << kIsolated
              << " (here: \"" << read_line(kIsolated) << "\") with sched_setaffinity\n";
#endif
    return 0;
}
