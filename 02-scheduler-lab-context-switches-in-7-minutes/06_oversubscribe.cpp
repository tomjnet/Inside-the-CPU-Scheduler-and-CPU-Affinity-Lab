// Context Switches in 7 Minutes: The Hidden Cost - slide 6: oversubscription: four threads, one core
// Build: make 06_oversubscribe
//
// One spinner alone for a second, then four spinners that must share one CPU for a second. Nobody
// blocks, so every switch is involuntary, and each thread reads its own counters from
// /proc/thread-self/status. The process restricts itself to one CPU with sched_setaffinity (a later
// episode explains the call; taskset -c 0 ./06_oversubscribe does the same from the shell).
// Without Linux the threads still run, spread over the free cores, and the counters read 0.
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <cerrno>
#include <cstring>
#include <sched.h>
#endif

struct Switches { long vol = 0; long invol = 0; };
static Switches read_switches() {       // this thread: one open, one scan
    Switches s;
    std::ifstream in("/proc/thread-self/status");
    for (std::string key; in >> key; )
        if (key == "voluntary_ctxt_switches:") in >> s.vol;
        else if (key == "nonvoluntary_ctxt_switches:") in >> s.invol;
    return s;
}

struct Report { std::uint64_t spins = 0; long forced = 0; };

// The baseline: one spinner, one second, wherever the scheduler puts it.
static Report spin_alone() {
    std::atomic<bool> stop{false};
    Report report;
    std::thread spinner([&] {
        Switches a = read_switches();
        std::uint64_t spins = 0;
        while (!stop.load(std::memory_order_relaxed)) ++spins;
        report = {spins, read_switches().invol - a.invol};
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    stop = true;
    spinner.join();
    return report;
}

// Restrict this thread, and every thread it creates afterwards, to the first CPU it is allowed on.
static bool restrict_to_one_cpu() {
#if defined(__linux__)
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) {
        std::cout << "sched_getaffinity refused: " << std::strerror(errno) << '\n';
        return false;
    }
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (!CPU_ISSET(cpu, &allowed)) continue;
        cpu_set_t one;
        CPU_ZERO(&one);
        CPU_SET(cpu, &one);
        if (sched_setaffinity(0, sizeof(one), &one) != 0) {
            std::cout << "sched_setaffinity refused: " << std::strerror(errno) << '\n';
            return false;
        }
        std::cout << "process restricted to CPU " << cpu << " (same as taskset -c " << cpu << ")\n";
        return true;
    }
    return false;
#else
    std::cout << "this sample needs Linux: it would squeeze the four spinners onto one CPU and count\n"
                 "their forced switches; here they spread over the free cores and the counters read 0\n";
    return false;
#endif
}

int main() {
    (void)read_switches();               // warm up: the first read of a /proc file allocates

    const Report alone = spin_alone();
    std::cout << "alone:     " << alone.spins << " spins, " << alone.forced << " forced switches\n";

    const bool one_cpu = restrict_to_one_cpu();

    // 4 spinners on 1 core: nobody blocks, every switch is forced
    std::atomic<bool> stop{false};
    std::vector<Report> reports(4);      // spins, forced switches
    std::vector<std::thread> pool;
    for (std::size_t id = 0; id < 4; ++id) pool.emplace_back([&, id] {
        Switches a = read_switches();    // the counters are per thread
        std::uint64_t spins = 0;
        while (!stop.load(std::memory_order_relaxed)) ++spins;
        reports[id] = {spins, read_switches().invol - a.invol};
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));   // bounded
    stop = true; for (auto& t : pool) t.join();

    long total_forced = 0;
    for (std::size_t id = 0; id < reports.size(); ++id) {
        const double share = alone.spins == 0 ? 0.0
            : 100.0 * static_cast<double>(reports[id].spins) / static_cast<double>(alone.spins);
        std::cout << "spinner " << id << ": " << reports[id].spins << " spins (" << share
                  << " percent of alone), " << reports[id].forced << " forced switches\n";
        total_forced += reports[id].forced;
    }
    std::cout << "this machine: " << total_forced << " forced switches in one second, "
              << (one_cpu ? "four threads on one CPU" : "threads not restricted to one CPU") << '\n';
    std::cout << "expect about 25 percent each and a few hundred switches when one CPU is shared\n";
    return 0;
}
