// Core Isolation in 9 Minutes: isolcpus and nohz_full - slide 10: count the ticks
// Build: make 10_count_ticks
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__linux__)
#include <sched.h>
#endif

static volatile std::uint64_t g_sink = 0;
static void sink(std::uint64_t x) { g_sink = g_sink + x; }   // keeps the result alive: the loop cannot be deleted

// The counters of one /proc/interrupts line: "LOC:  250  1000  3  Local timer interrupts" gives {250, 1000, 3}.
static std::vector<long long> parse_counters(const std::string& line) {
    std::vector<long long> counters;
    std::size_t colon = line.find(':');
    if (colon == std::string::npos) return counters;
    std::istringstream rest(line.substr(colon + 1));
    long long value = 0;
    while (rest >> value) counters.push_back(value);           // stops at the first word of the description
    return counters;
}

// One counter per CPU from the line that starts with `name` in /proc/interrupts, empty when there is none.
static std::vector<long long> read_line_named(const std::string& name) {
    std::ifstream in("/proc/interrupts");
    std::string line;
    while (in && std::getline(in, line)) {
        std::size_t start = line.find_first_not_of(' ');
        if (start != std::string::npos && line.compare(start, name.size(), name) == 0)
            return parse_counters(line);
    }
    return {};
}

static std::vector<long long> read_loc_line() { return read_line_named("LOC:"); }

// A bounded busy loop: keeps this CPU busy until the deadline, never longer.
static void spin_for(std::chrono::steady_clock::duration d) {
    auto deadline = std::chrono::steady_clock::now() + d;
    std::uint64_t n = 0;
    while (std::chrono::steady_clock::now() < deadline) sink(++n);
}

int main() {
#if defined(__linux__)
    // Stay on the CPU we started on, so the two reads count the same core.
    int here = sched_getcpu();
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(here < 0 ? 0 : here, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) std::cout << "could not pin, the CPU may change\n";
    std::cout << "warm up read: " << read_loc_line().size() << " LOC columns\n";   // page faults happen here

    std::vector<long long> hvs_before = read_line_named("HVS:");
    // LOC in /proc/interrupts: local timer interrupts, one column per CPU.
    // The spin keeps our CPU busy: an idle CPU stops its tick anyway.
    std::vector<long long> before = read_loc_line();
    spin_for(std::chrono::seconds(1));           // bounded busy loop
    std::vector<long long> after = read_loc_line();
    std::size_t cpu = static_cast<std::size_t>(sched_getcpu());
    std::vector<long long> hvs_after = read_line_named("HVS:");
    if (cpu >= before.size() || cpu >= after.size()) {
        std::cout << "no LOC column for CPU " << cpu << " in /proc/interrupts: nothing to count here\n";
        return 0;
    }
    std::cout << "CPU " << cpu << ": " << after[cpu] - before[cpu]
              << " timer interrupts in one second\n";
    // Housekeeping CPU: about HZ, 250 or 1000. nohz_full CPU running
    // only this task: a handful. Same program, different core.

    if (after[cpu] - before[cpu] == 0 && cpu < hvs_before.size() && cpu < hvs_after.size())
        std::cout << "LOC is zero: this is a Hyper-V guest (WSL) and its tick is the HVS line: "
                  << hvs_after[cpu] - hvs_before[cpu] << " in one second. Take the real number on hardware.\n";
    std::cout << "on real hardware expect about HZ (250 or 1000) on a housekeeping CPU, a handful on a "
                 "nohz_full CPU\n";
#else
    std::cout << "this sample needs Linux: it would count the LOC timer interrupts of its own busy CPU\n";
    // The parser and the arithmetic still run, on two typical lines taken one second apart.
    std::vector<long long> before = parse_counters("LOC:  881200  450100  9031  Local timer interrupts");
    spin_for(std::chrono::milliseconds(100));
    std::vector<long long> after = parse_counters("LOC:  881450  450350  9033  Local timer interrupts");
    std::cout << "/proc/interrupts here: " << read_loc_line().size() << " LOC columns\n";
    for (std::size_t cpu = 0; cpu < before.size() && cpu < after.size(); ++cpu)
        std::cout << "typical CPU " << cpu << ": " << after[cpu] - before[cpu] << " timer interrupts in one second"
                  << (cpu == 2 ? "  (nohz_full)" : "  (housekeeping, HZ=250)") << "\n";
#endif
    return 0;
}
