// Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 6: percentiles: sort once, index four times
// Build: make 06_percentiles
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <time.h>
#endif

static constexpr int kSamples = 5000;                 // 5000 wakeups, 1 ms apart: about 5 seconds
static constexpr long long kPeriodNs = 1'000'000;     // 1 ms

// The loop of slide 5: sleep to an absolute deadline, record how late each wakeup is, in nanoseconds.
static std::vector<long long> measure(int samples) {
    std::vector<long long> late_ns;
    late_ns.reserve(static_cast<std::size_t>(samples));
#if defined(__linux__)
    timespec next{};
    clock_gettime(CLOCK_MONOTONIC, &next);
    for (int i = 0; i < samples; ++i) {
        next.tv_nsec += static_cast<long>(kPeriodNs);
        if (next.tv_nsec >= 1'000'000'000L) {
            next.tv_nsec -= 1'000'000'000L;
            ++next.tv_sec;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);
        timespec now{};
        clock_gettime(CLOCK_MONOTONIC, &now);
        late_ns.push_back(static_cast<long long>(now.tv_sec - next.tv_sec) * 1'000'000'000LL
                          + (now.tv_nsec - next.tv_nsec));
    }
#else
    auto next = std::chrono::steady_clock::now();     // portable fallback: same grid, sleep_until
    for (int i = 0; i < samples; ++i) {
        next += std::chrono::nanoseconds(kPeriodNs);
        std::this_thread::sleep_until(next);
        auto now = std::chrono::steady_clock::now();
        late_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(now - next).count());
    }
#endif
    return late_ns;
}

// nearest rank on a sorted vector: the index is the percentile
long long percentile(const std::vector<long long>& sorted, double p) {
    double pos = p / 100.0 * static_cast<double>(sorted.size());
    auto rank = static_cast<std::size_t>(pos);
    return sorted[std::min(rank, sorted.size() - 1)];
}

int main() {
#if !defined(__linux__)
    std::cout << "this sample needs Linux: clock_nanosleep wakeup latency; using sleep_until instead\n";
#endif
    std::vector<long long> late = measure(kSamples);   // 5000 x 1 ms
    std::sort(late.begin(), late.end());        // O(n log n), once
    long long p50  = percentile(late, 50.0);    // the typical wakeup
    long long p99  = percentile(late, 99.0);    // 50 samples are worse
    long long p999 = percentile(late, 99.9);    // 5 samples are worse
    long long worst = late.back();              // the one that hurts

    std::cout << "wakeup latency of " << late.size() << " samples, this machine\n";
    std::cout << "  p50   : " << p50 / 1000 << " us\n";
    std::cout << "  p99   : " << p99 / 1000 << " us   (" << late.size() / 100 << " samples are worse)\n";
    std::cout << "  p99.9 : " << p999 / 1000 << " us   (" << late.size() / 1000 << " samples are worse)\n";
    std::cout << "  max   : " << worst / 1000 << " us\n";
    if (p50 > 0)
        std::cout << "  tail ratio p99.9 / p50: " << p999 / p50 << "x\n";
    std::cout << "report the four together, with the sample count; never average percentiles\n";

    // correctness: percentiles of a sorted vector can only grow
    bool ordered = p50 <= p99 && p99 <= p999 && p999 <= worst;
    std::cout << (ordered ? "check: p50 <= p99 <= p99.9 <= max\n" : "check FAILED: percentiles out of order\n");
    return ordered ? 0 : 1;
}
