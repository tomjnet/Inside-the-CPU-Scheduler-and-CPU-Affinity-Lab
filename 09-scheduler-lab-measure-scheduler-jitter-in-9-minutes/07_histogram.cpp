// Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 7: the histogram: buckets that double
// Build: make 07_histogram
#include <array>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
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

// one row: lower edge of the bucket, count, and a bar scaled to 50 columns for all the samples
static void print_row(long long from_us, int count) {
    int width = count * 50 / kSamples;
    if (count > 0 && width == 0) width = 1;               // a tail of one sample must stay visible
    std::cout << std::setw(8) << from_us << " us | " << std::setw(5) << count << " | "
              << std::string(static_cast<std::size_t>(width), '#') << "\n";
}

int main() {
#if !defined(__linux__)
    std::cout << "this sample needs Linux: clock_nanosleep wakeup latency; using sleep_until instead\n";
#endif
    std::vector<long long> late = measure(kSamples);
    std::cout << "wakeup latency histogram, " << late.size() << " samples, this machine\n";
    std::cout << "    from    | count | each bucket covers twice the range of the previous one\n";

    // bucket b holds latencies from 2^b to 2^(b+1) microseconds
    std::array<int, 16> bucket{};
    for (long long ns : late) {
        long long us = ns / 1000;
        std::size_t b = 0;
        while (us > 1 && b < 15) { us /= 2; ++b; }   // integer log2
        ++bucket[b];
    }
    for (std::size_t b = 0; b < bucket.size(); ++b)  // one row per bucket
        print_row(1LL << b, bucket[b]);              // from, count, bar
    // the tail: short rows far below the tallest one

    int total = 0;
    for (int c : bucket) total += c;
    std::cout << "one narrow hump: healthy. a second hump: a periodic disturbance. a long thin tail: rare events\n";
    bool complete = total == static_cast<int>(late.size());
    std::cout << (complete ? "check: every sample landed in a bucket\n" : "check FAILED: samples lost\n");
    return complete ? 0 : 1;
}
