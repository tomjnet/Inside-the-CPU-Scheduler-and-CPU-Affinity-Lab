// CPU Migration and Cache Warmth in 7 Minutes - slide 7: the same worker under load
// Build: make 07_under_load
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <fstream>
#include <sched.h>
#include <string>
#endif

// how often two neighbouring samples differ: each change is one wakeup on another CPU
static int count_changes(const std::vector<int>& trace) {
    int changes = 0;
    for (std::size_t i = 1; i < trace.size(); ++i)
        changes += trace[i] != trace[i - 1];
    return changes;
}

#if defined(__linux__)
// the recorder of slide 6: one sched_getcpu() per millisecond, n samples
static std::vector<int> record_cpus(int n) {
    std::vector<int> trace;
    trace.reserve(static_cast<std::size_t>(n));
    auto next = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) {
        trace.push_back(sched_getcpu());
        next += std::chrono::milliseconds(1);
        std::this_thread::sleep_until(next);
    }
    return trace;
}

// a short burst of work that the compiler cannot delete
static void spin() {
    volatile unsigned x = 0;
    for (int i = 0; i < 1000; ++i) x = x + 1;
}

// se.nr_migrations of the main thread from /proc/self/sched, or -1 when the kernel does not expose it
static long read_nr_migrations() {
    std::ifstream in("/proc/self/sched");
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("nr_migrations") == std::string::npos) continue;
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) return -1;
        try {
            return std::stol(line.substr(colon + 1));
        } catch (...) {
            return -1;
        }
    }
    return -1;
}
#endif

int main() {
    const unsigned cpus = std::thread::hardware_concurrency();
    std::cout << "logical CPUs: " << cpus << ", threads in the loaded run: 3 spinners + 1 worker\n";
    if (cpus > 4)
        std::cout << "more CPUs than threads: an idle CPU is easy to find, so the old one is often still free\n";
    else
        std::cout << "as many threads as CPUs or more: the old CPU is often busy when the worker wakes up\n";
    std::cout << "model: trace 1 1 3 3 1 has " << count_changes({1, 1, 3, 3, 1}) << " changes\n\n";

#if defined(__linux__)
    (void)read_nr_migrations();                    // first read opens and faults in what it needs
    const long m0 = read_nr_migrations();

    int idle = count_changes(record_cpus(1000));   // quiet machine
    const long m1 = read_nr_migrations();
    std::atomic<bool> stop{false};
    std::vector<std::thread> load;        // 3 spinners + the worker
    for (int i = 0; i < 3; ++i)
        load.emplace_back([&stop] { while (!stop) spin(); });
    int busy = count_changes(record_cpus(1000));   // bounded: 1 second
    stop = true;
    for (auto& t : load) t.join();
    // old CPU busy at wakeup: the worker lands on another core,
    // and every landing starts with a cold L1 and L2
    std::cout << "changes: idle " << idle << ", loaded " << busy << '\n';

    const long m2 = read_nr_migrations();
    if (m0 >= 0 && m1 >= 0 && m2 >= 0)
        std::cout << "se.nr_migrations of this thread (the kernel's own count): idle run +" << (m1 - m0)
                  << ", loaded run +" << (m2 - m1) << '\n';
    else
        std::cout << "se.nr_migrations: /proc/self/sched is not available on this kernel\n";
    std::cout << "nothing is asserted: the scheduler owes us no particular number, and it differs per run.\n"
              << "on real hardware with more load than cores the loaded count is often many times the idle one\n";
#else
    std::cout << "this sample needs Linux: it would record sched_getcpu() for one second on a quiet machine,\n"
              << "again next to three spinner threads, and print both counts of CPU changes\n";
#endif
    return 0;
}
