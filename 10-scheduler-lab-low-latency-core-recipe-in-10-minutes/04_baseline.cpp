// A Repeatable Low Latency Core Recipe in 10 Minutes - slide 4: stage 1: the baseline probe
// Build: make 04_baseline
// Run:   ./04_baseline [samples]   (default 2000, one wakeup per millisecond; use 5000 or more for a real baseline)
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <vector>

#if defined(__linux__)
#include <time.h>

static long long to_ns(const timespec& t) { return t.tv_sec * 1'000'000'000LL + t.tv_nsec; }
static timespec now_ts() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t;
}
static long long now_ns() { return to_ns(now_ts()); }
static void add_ns(timespec& t, long ns) {
    t.tv_nsec += ns;
    while (t.tv_nsec >= 1'000'000'000L) {
        t.tv_nsec -= 1'000'000'000L;
        ++t.tv_sec;
    }
}

// Stage 1, the episode 9 probe: how late does a 1 ms wakeup arrive?
std::vector<long long> measure(std::size_t n) {
    std::vector<long long> late(n);        // allocate before the loop
    timespec next = now_ts();              // CLOCK_MONOTONIC
    for (auto& ns : late) {
        add_ns(next, 1'000'000);           // absolute deadline: +1 ms
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);
        ns = now_ns() - to_ns(next);       // overshoot of this wakeup
    }
    std::sort(late.begin(), late.end());   // sorted: percentile = index
    return late;
}
static const char* kProbe = "clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)";
#else
#include <chrono>
#include <thread>

// Portable stand-in for the slide's probe: the same absolute deadlines with std::chrono. The default Windows
// timer is coarse (up to about 15 ms), so expect a much larger overshoot here than on Linux.
std::vector<long long> measure(std::size_t n) {
    std::vector<long long> late(n);        // allocate before the loop
    auto next = std::chrono::steady_clock::now();
    for (auto& ns : late) {
        next += std::chrono::milliseconds(1);              // absolute deadline: +1 ms
        std::this_thread::sleep_until(next);
        ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - next).count();
    }
    std::sort(late.begin(), late.end());   // sorted: percentile = index
    return late;
}
static const char* kProbe = "std::this_thread::sleep_until (this sample needs Linux for the clock_nanosleep probe)";
#endif

int main(int argc, char** argv) {
    std::size_t n = 2000;
    if (argc > 1) {
        long asked = std::atol(argv[1]);
        if (asked >= 10 && asked <= 600000) n = static_cast<std::size_t>(asked);
    }
    std::printf("probe: %s\n", kProbe);
    std::printf("samples: %zu wakeups, one per millisecond, nothing tuned\n", n);

    std::vector<long long> late = measure(n);

    auto at = [&](std::size_t permille) { return late[(late.size() - 1) * permille / 1000] / 1000; };
    std::printf("baseline on this machine: p50 %lld us, p99 %lld us, p99.9 %lld us, max %lld us\n",
                at(500), at(990), at(999), at(1000));
    std::printf("write this line in the log before you change anything: every later stage is compared with it\n");
    return 0;
}
