// CPU Migration and Cache Warmth in 7 Minutes - slide 6: record the cpu each millisecond
// Build: make 06_record_cpu
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <sched.h>
#endif

static volatile std::uint64_t g_sink = 0;
static void sink(std::uint64_t x) { g_sink = g_sink + x; }   // keeps the result alive: the loop cannot be deleted

// the working set: reads every word, so the lines must be in the cache of the CPU we run on
static std::uint64_t touch(const std::vector<std::uint64_t>& w) {
    std::uint64_t sum = 0;
    for (std::uint64_t x : w) sum += x;
    return sum;
}

static double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

static double touch_ns(const std::vector<std::uint64_t>& w) {
    auto t0 = std::chrono::steady_clock::now();
    sink(touch(w));
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

int main() {
    // 256 KiB of 64 bit words: larger than a typical L1, small enough for a typical L2
    static std::vector<std::uint64_t> work(32768);
    for (std::size_t i = 0; i < work.size(); ++i) work[i] = i * 2654435761ULL;
    std::cout << "working set: " << work.size() * sizeof(std::uint64_t) / 1024 << " KiB, "
              << work.size() * sizeof(std::uint64_t) / 64 << " cache lines of 64 bytes\n";

    // portable part: how warm the working set is when nothing moves (touched back to back, no sleep)
    std::vector<double> warm;
    for (int i = 0; i < 3; ++i) sink(touch(work));             // warm up
    for (int i = 0; i < 21; ++i) warm.push_back(touch_ns(work));
    std::cout << "one warm touch, back to back: median " << median(warm) << " ns (this machine)\n";

    // portable part: what "count the changes" means, on a trace written by hand
    const std::vector<int> model{2, 2, 2, 5, 5, 5, 2, 2};
    int model_changes = 0;
    for (std::size_t i = 1; i < model.size(); ++i)
        model_changes += model[i] != model[i - 1];
    std::cout << "model trace 2 2 2 5 5 5 2 2: " << model_changes << " CPU changes, 2 distinct CPUs\n\n";

#if defined(__linux__)
    std::vector<double> ns;                   // added: the time of every touch, for the comparison below
    ns.reserve(1000);

    std::vector<int> trace;               // 1000 samples, 1 ms apart
    auto next = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        sink(touch(work));                // keep a working set warm
        auto t1 = std::chrono::steady_clock::now();
        ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
        trace.push_back(sched_getcpu());  // vDSO: no system call
        next += std::chrono::milliseconds(1);
        std::this_thread::sleep_until(next);  // wakeup = new placement
    }
    int changes = 0;                      // each change: one cold cache
    for (std::size_t i = 1; i < trace.size(); ++i)
        changes += trace[i] != trace[i - 1];
    std::cout << "CPU changes in 1 s: " << changes << '\n';

    if (std::count(trace.begin(), trace.end(), -1) > 0)
        std::cout << "note: sched_getcpu returned -1 for some samples (not supported here)\n";

    const std::set<int> distinct(trace.begin(), trace.end());
    std::cout << "distinct CPUs visited: " << distinct.size() << " (";
    for (int cpu : distinct) std::cout << ' ' << cpu;
    std::cout << " )\n";

    // the touch of sample i ran just before sched_getcpu of sample i: if the CPU differs from sample i - 1,
    // that touch was the first one on the new CPU
    std::vector<double> after_change;
    std::vector<double> same_cpu;
    for (std::size_t i = 1; i < trace.size(); ++i)
        (trace[i] != trace[i - 1] ? after_change : same_cpu).push_back(ns[i]);
    std::cout << "touch after a 1 ms sleep, same CPU:   median " << median(same_cpu) << " ns ("
              << same_cpu.size() << " samples, this machine)\n";
    if (after_change.empty()) {
        std::cout << "touch right after a CPU change:       no change seen in this run\n"
                  << "  an idle machine often keeps a sleeper on its old CPU: run 07_under_load, or start other load\n";
    } else {
        std::cout << "touch right after a CPU change:       median " << median(after_change) << " ns ("
                  << after_change.size() << " samples, this machine)\n"
                  << "  ratio " << median(after_change) / median(same_cpu)
                  << "x: a few samples are noisy, and a hop to an SMT sibling costs almost nothing\n";
    }
    std::cout << "the count is a lower bound: a hop there and back inside one millisecond is invisible\n";
#else
    std::cout << "this sample needs Linux: it would record sched_getcpu() each millisecond for one second\n"
              << "and print the number of CPU changes, the CPUs visited and the touch time after a change\n";
#endif
    return 0;
}
