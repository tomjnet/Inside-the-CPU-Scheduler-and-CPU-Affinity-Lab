// Pinning with taskset in 6 Minutes: One Process, One Core - slide 7: pin the benchmark, compare the spread
// Build: make 07_spread
// Try:   ./07_spread    then    taskset -c 2 ./07_spread    (a few times each, ideally on a busy machine)
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <set>
#include <vector>

#if defined(__linux__)
#include <sched.h>
#endif

static volatile std::uint64_t g_sink = 0;
static void sink(std::uint64_t x) { g_sink = g_sink + x; }   // keeps the result alive: the loop cannot be deleted

using Clock = std::chrono::steady_clock;
static Clock::time_point now() { return Clock::now(); }
static double ns_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::nano>(Clock::now() - t0).count();
}

// Fixed work: n steps of a xorshift generator. Same input, same steps, every run.
static std::uint64_t work(int n) {
    std::uint64_t x = 88172645463325252ull, sum = 0;
    for (int i = 0; i < n; ++i) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        sum += x;
    }
    return sum;
}

int main() {
    for (int i = 0; i < 20; ++i) sink(work(200'000));   // warm up: caches, branch predictor, clock speed

    // Same work 200 times: the spread is the machine, not the code
    std::vector<double> ns;
    std::set<int> cpus;                       // CPUs this process visited
    for (int run = 0; run < 200; ++run) {
        auto t0 = now();
        sink(work(200'000));                  // fixed work per run
        ns.push_back(ns_since(t0));
#if defined(__linux__)
        cpus.insert(sched_getcpu());          // pinned: always one CPU
#endif
    }

    std::sort(ns.begin(), ns.end());
    const std::size_t n = ns.size();
    const double lo = ns.front(), median = ns[n / 2], p99 = ns[n * 99 / 100], hi = ns.back();
    std::cout << "200 runs of the same work, this machine, microseconds per run\n";
    std::cout << "  min    " << lo / 1000.0 << '\n';
    std::cout << "  median " << median / 1000.0 << '\n';
    std::cout << "  p99    " << p99 / 1000.0 << '\n';
    std::cout << "  max    " << hi / 1000.0 << '\n';
    std::cout << "  max / median = " << hi / median << "   p99 / median = " << p99 / median << '\n';

    if (cpus.empty()) {
        std::cout << "CPUs visited: this part needs Linux (sched_getcpu): it would list every CPU the runs "
                     "landed on, exactly one under taskset -c 2\n";
    } else {
        std::cout << "CPUs visited: " << cpus.size() << " (";
        bool first = true;
        for (int cpu : cpus) {
            std::cout << (first ? "" : " ") << cpu;
            first = false;
        }
        std::cout << ")\n";
        std::cout << (cpus.size() == 1 ? "one CPU for every run: pinned, or simply never migrated this time\n"
                                       : "migrated between runs: try again under  taskset -c 2\n");
    }
    std::cout << "compare the tail (p99, max), not the median, and repeat: one run proves nothing\n";
    return 0;
}
