// Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 9: correlate: which outliers moved or were preempted
// Build: make 09_correlate
#include <algorithm>
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
#include <sched.h>
#include <time.h>
#endif

static constexpr int kSamples = 5000;                 // 5000 wakeups, 1 ms apart: about 5 seconds
static constexpr long long kPeriodNs = 1'000'000;     // 1 ms

static volatile std::uint64_t g_sink = 0;
static void sink(std::uint64_t x) { g_sink = g_sink + x; }   // keeps the loop of the spinners alive

struct Sample { long long late_ns; int cpu; };        // how late, and on which CPU we woke up
struct Switches { long voluntary; long involuntary; };

// episode 2: the two context switch counters of this thread, from /proc/self/status (zeros when missing)
static Switches read_switches() {
    Switches s{0, 0};
    std::ifstream status("/proc/self/status");
    std::string key;
    while (status >> key) {
        if (key == "voluntary_ctxt_switches:") status >> s.voluntary;
        else if (key == "nonvoluntary_ctxt_switches:") status >> s.involuntary;
    }
    return s;
}

// the loop of slide 5, with sched_getcpu() after every wakeup
static std::vector<Sample> measure_with_cpu(int samples) {
    std::vector<Sample> run;
    run.reserve(static_cast<std::size_t>(samples));
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
        long long late = static_cast<long long>(now.tv_sec - next.tv_sec) * 1'000'000'000LL
                         + (now.tv_nsec - next.tv_nsec);
        run.push_back(Sample{late, sched_getcpu()});
    }
#else
    auto next = std::chrono::steady_clock::now();     // portable fallback: no CPU number, always 0
    for (int i = 0; i < samples; ++i) {
        next += std::chrono::nanoseconds(kPeriodNs);
        std::this_thread::sleep_until(next);
        auto now = std::chrono::steady_clock::now();
        run.push_back(Sample{std::chrono::duration_cast<std::chrono::nanoseconds>(now - next).count(), 0});
    }
#endif
    return run;
}

int main() {
#if !defined(__linux__)
    std::cout << "this sample needs Linux: sched_getcpu per wakeup and the counters of /proc/self/status\n";
#endif
    // interference on purpose: three unpinned spinners, so the load balancer has a reason to move us
    std::atomic<bool> stop{false};
    std::vector<std::thread> spinners;
    for (int i = 0; i < 3; ++i)
        spinners.emplace_back([&stop] { while (!stop.load(std::memory_order_relaxed)) sink(1); });

    Switches warm = read_switches();       // warm up read: /proc itself allocates and can fault in pages
    long long p99 = 0;                     // from a short probe run, so the loop below has its threshold
    {
        std::vector<Sample> probe = measure_with_cpu(500);
        std::vector<long long> sorted;
        sorted.reserve(probe.size());
        for (const Sample& s : probe) sorted.push_back(s.late_ns);
        std::sort(sorted.begin(), sorted.end());
        p99 = sorted[sorted.size() * 99 / 100];
    }
    std::cout << "warm up: " << warm.voluntary << " voluntary switches so far, p99 of a 500 sample probe: "
              << p99 / 1000 << " us\n";

    // every sample keeps the CPU it woke up on: sched_getcpu()
    Switches before = read_switches();     // /proc/self/status, episode 2
    std::vector<Sample> run = measure_with_cpu(kSamples);
    Switches after = read_switches();
    int migrations = 0, late_and_moved = 0;
    for (std::size_t i = 1; i < run.size(); ++i) {
        bool moved = run[i].cpu != run[i - 1].cpu;       // episode 3
        if (moved) ++migrations;
        if (moved && run[i].late_ns > p99) ++late_and_moved;
    }
    long preempted = after.involuntary - before.involuntary;
    // voluntary grows by one per sleep: only involuntary is noise

    stop.store(true);
    for (std::thread& t : spinners) t.join();

    int late_total = 0;
    for (const Sample& s : run)
        if (s.late_ns > p99) ++late_total;
    std::cout << run.size() << " wakeups next to 3 spinners, this machine\n";
    std::cout << "  voluntary switches  : " << after.voluntary - before.voluntary << "   (one per sleep: expected)\n";
    std::cout << "  involuntary switches: " << preempted << "   (the scheduler took the CPU away)\n";
    std::cout << "  migrations          : " << migrations << "   (the wakeup CPU changed)\n";
    std::cout << "  samples above p99   : " << late_total << ", of which " << late_and_moved << " woke on a new CPU\n";
    std::cout << "outliers that neither moved nor were preempted: look at /proc/interrupts (episode 8)\n";
    return 0;
}
