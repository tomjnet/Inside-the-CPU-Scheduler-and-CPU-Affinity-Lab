// Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 5: the measurement loop: an absolute deadline
// Build: make 05_absolute_deadline
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <time.h>
#endif

static constexpr int kSamples = 5000;                 // 5000 wakeups, 1 ms apart: about 5 seconds
static constexpr long long kPeriodNs = 1'000'000;     // 1 ms

#if defined(__linux__)
static void add_ns(timespec& t, long long ns) {
    t.tv_nsec += static_cast<long>(ns);
    while (t.tv_nsec >= 1'000'000'000L) {
        t.tv_nsec -= 1'000'000'000L;
        ++t.tv_sec;
    }
}

static long long diff_ns(const timespec& a, const timespec& b) {
    return static_cast<long long>(a.tv_sec - b.tv_sec) * 1'000'000'000LL + (a.tv_nsec - b.tv_nsec);
}
#endif

int main() {
    std::vector<long long> late_ns;
    late_ns.reserve(static_cast<std::size_t>(kSamples));   // no allocation inside the loop

#if defined(__linux__)
    // ask to wake at an absolute deadline, measure how late we are
    timespec next{};
    clock_gettime(CLOCK_MONOTONIC, &next);         // start of the grid
    for (int i = 0; i < kSamples; ++i) {           // 5000 wakeups
        add_ns(next, kPeriodNs);                   // deadline += 1 ms
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);
        timespec now{};
        clock_gettime(CLOCK_MONOTONIC, &now);      // awake: read the clock
        late_ns.push_back(diff_ns(now, next));     // wakeup latency
    }
    // one voluntary context switch per sample, and no drift
    std::cout << "clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME), " << kSamples << " wakeups, 1 ms apart\n";
#else
    // portable fallback: the same absolute deadline grid with the C++ standard library
    std::cout << "this sample needs Linux: clock_nanosleep with TIMER_ABSTIME and its wakeup latency\n";
    std::cout << "portable fallback: std::this_thread::sleep_until on the same 1 ms grid\n";
    auto next = std::chrono::steady_clock::now();
    for (int i = 0; i < kSamples; ++i) {
        next += std::chrono::nanoseconds(kPeriodNs);
        std::this_thread::sleep_until(next);
        auto now = std::chrono::steady_clock::now();
        late_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(now - next).count());
    }
#endif

    long long lo = *std::min_element(late_ns.begin(), late_ns.end());
    long long hi = *std::max_element(late_ns.begin(), late_ns.end());
    long long sum = std::accumulate(late_ns.begin(), late_ns.end(), 0LL);
    std::cout << "samples: " << late_ns.size() << " (this machine)\n";
    std::cout << "  min wakeup latency: " << lo / 1000 << " us\n";
    std::cout << "  avg wakeup latency: " << sum / static_cast<long long>(late_ns.size()) / 1000 << " us\n";
    std::cout << "  max wakeup latency: " << hi / 1000 << " us\n";
    std::cout << "  jitter (max - min): " << (hi - lo) / 1000 << " us\n";
    std::cout << "a normal thread pays timer slack (50 us by default) on top of the scheduler\n";
    std::cout << "min, avg and max hide the shape: 06_percentiles and 07_histogram show it\n";
    return 0;
}
