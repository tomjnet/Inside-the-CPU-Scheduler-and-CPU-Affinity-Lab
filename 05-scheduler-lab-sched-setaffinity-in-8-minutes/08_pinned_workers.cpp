// CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 8: the benchmark: four workers, free and pinned
// Build: make 08_pinned_workers
// Usage: ./08_pinned_workers [repeats] [pinned]   (09_verify.sh runs a long pinned pass to inspect from outside)
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sched.h>
#endif

static volatile std::uint64_t g_sink = 0;
static void sink(std::uint64_t x) { g_sink = g_sink + x; }   // keeps the result alive: the loop cannot be deleted

struct BenchResult { double min_ns; double median_ns; };

static int g_repeats = 201;                                   // a couple of hundred runs per worker

template <class F>
static BenchResult bench_ns(F&& f, int warmup = 3, int repeats = g_repeats) {
    for (int i = 0; i < warmup; ++i) f();                     // warm up: caches, branch predictor, page faults
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(repeats));
    for (int i = 0; i < repeats; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        f();
        auto t1 = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
    }
    std::sort(samples.begin(), samples.end());
    return BenchResult{samples.front(), samples[samples.size() / 2]};
}

#if defined(__linux__)
static bool pin_self(int target) {
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(target, &one);
    if (sched_setaffinity(0, sizeof one, &one) != 0) {
        fprintf(stderr, "pin failed: %s\n", strerror(errno));
        return false;
    }
    return sched_getcpu() == target;
}
static std::vector<int> allowed_cpus() {
    std::vector<int> cpus;
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof allowed, &allowed) != 0) return cpus;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
        if (CPU_ISSET(cpu, &allowed)) cpus.push_back(cpu);
    return cpus;
}
#else
// stand-ins so the free pass still runs and prints timings on other systems
static bool pin_self(int) { return false; }
static int sched_getcpu() { return 0; }
static std::vector<int> allowed_cpus() { return std::vector<int>{0}; }
#endif

static const std::size_t kWorkers = 4;
static const std::size_t kElements = std::size_t{1} << 20;   // 4 MiB per worker: about a millisecond or less per sum
static std::vector<std::vector<std::uint32_t>> data;
static std::mutex print_mutex;

static std::uint64_t sum_squares(const std::vector<std::uint32_t>& v) {
    std::uint64_t s = 0;
    for (std::uint32_t x : v) s += static_cast<std::uint64_t>(x) * x;
    return s;
}

static void report(std::size_t id, int last, int moves, const BenchResult& r) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << "  worker " << id << ": last CPU " << last << ", moves " << moves << ", min " << r.min_ns / 1000.0
              << " us, median " << r.median_ns / 1000.0 << " us\n";
}

// one worker: time the same job and count the CPU changes
void worker(std::size_t id, bool pin, const std::vector<int>& cpus) {
    if (pin) pin_self(cpus[id % cpus.size()]);   // one CPU each
    int last = sched_getcpu(), moves = 0;
    auto r = bench_ns([&] {                  // harness: min and median
        sink(sum_squares(data[id]));         // about 1 ms of work
        int now = sched_getcpu();            // vDSO: no system call
        if (now != last) { ++moves; last = now; }
    });
    report(id, last, moves, r);              // pinned: moves is 0
}

static void pass(const char* title, bool pin, const std::vector<int>& cpus) {
    std::cout << title << "\n";
    std::vector<std::thread> threads;
    for (std::size_t id = 0; id < kWorkers; ++id) threads.emplace_back(worker, id, pin, std::cref(cpus));
    for (std::thread& t : threads) t.join();
}

int main(int argc, char** argv) {
    if (argc > 1) g_repeats = std::max(1, std::atoi(argv[1]));
    bool pinned_only = argc > 2 && std::string(argv[2]) == "pinned";

    std::mt19937 rng(12345);                                  // fixed seed: every run does the same work
    for (std::size_t w = 0; w < kWorkers; ++w) {
        std::vector<std::uint32_t> v(kElements);
        for (std::uint32_t& x : v) x = static_cast<std::uint32_t>(rng() & 0xFFFFu);
        data.push_back(std::move(v));
    }

    std::vector<int> cpus = allowed_cpus();
    if (cpus.empty()) cpus.push_back(0);
    std::cout << cpus.size() << " allowed CPUs, " << kWorkers << " workers, " << g_repeats << " timed runs each (this machine)\n";

    if (!pinned_only) pass("free: the scheduler places and moves the workers", false, cpus);
#if defined(__linux__)
    pass("pinned: worker i on allowed CPU i, moves must be 0", true, cpus);
    std::cout << "quiet machine? the free workers may not move either: start a compile next to it and run again\n";
#else
    std::cout << "this sample needs Linux: the pinned pass with sched_setaffinity and the moves counted by sched_getcpu\n";
#endif
    return 0;
}
