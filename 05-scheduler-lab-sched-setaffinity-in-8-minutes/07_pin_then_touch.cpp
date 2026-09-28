// CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 7: startup order: pin first, allocate after
// Build: make 07_pin_then_touch
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numeric>
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

template <class F>
static BenchResult bench_ns(F&& f, int warmup = 3, int repeats = 21) {
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
static int current_cpu() { return sched_getcpu(); }
static int pick_cpu() {                                       // second allowed CPU, or the only one
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof allowed, &allowed) != 0) return 0;
    std::vector<int> cpus;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
        if (CPU_ISSET(cpu, &allowed)) cpus.push_back(cpu);
    return cpus.empty() ? 0 : cpus[1 % cpus.size()];
}
#else
static bool pin_self(int) {
    std::cout << "this sample needs Linux: sched_setaffinity before the allocation; the order below still runs, unpinned\n";
    return false;
}
static int current_cpu() { return -1; }
static int pick_cpu() { return 0; }
#endif

static std::atomic<bool> ready{false};

static std::uint64_t sum(const std::vector<std::uint64_t>& v) {
    return std::accumulate(v.begin(), v.end(), std::uint64_t{0});
}

static void run(const std::vector<std::uint64_t>& data) {
    BenchResult r = bench_ns([&] { sink(sum(data)); });
    std::cout << "hot loop on CPU " << current_cpu() << ": sum of " << data.size() << " elements, min "
              << r.min_ns / 1000.0 << " us, median " << r.median_ns / 1000.0 << " us (this machine)\n";
}

void worker(int cpu, std::size_t n) {
    pin_self(cpu);                           // 1. pin before allocating
    std::vector<std::uint64_t> data(n, 1);   // 2. allocate and touch:
                                             //    pages land near cpu
    for (int i = 0; i < 3; ++i) sink(sum(data));    // 3. warm up
    ready.store(true);                       // 4. only now take work
    run(data);                               // same CPU, warm cache
}

int main() {
    int cpu = pick_cpu();
    std::size_t n = std::size_t{1} << 20;                     // 8 MiB of uint64_t: 2048 pages to place
    std::cout << "main runs on CPU " << current_cpu() << ", worker will pin to CPU " << cpu << " and only then allocate\n";

    std::thread t(worker, cpu, n);
    while (!ready.load()) std::this_thread::yield();          // the ready flag rises after pin, touch and warm up
    t.join();                                                 // print after the join: one writer at a time
    std::cout << "main saw the ready flag only after the pin, the first touch and the warm up\n";

    std::cout << "on one memory node (a laptop, WSL) the order costs only cache warmth;\n"
              << "on two sockets check the placement with: numastat -p <pid>\n";
    return 0;
}
