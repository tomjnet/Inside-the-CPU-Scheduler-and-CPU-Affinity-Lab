// Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 8: three runs: alone, competing, pinned apart
// Build: make 08_three_runs
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <cerrno>
#include <cstring>
#include <sched.h>
#include <time.h>
#endif

static volatile std::uint64_t g_sink = 0;
static void sink(std::uint64_t x) { g_sink = g_sink + x; }   // keeps the spinner's loop alive

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

// the CPUs this process may use (episode 4): never a hard coded number
static std::vector<int> allowed_cpus() {
    std::vector<int> cpus;
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) == 0)
        for (int c = 0; c < CPU_SETSIZE; ++c)
            if (CPU_ISSET(c, &set)) cpus.push_back(c);
#endif
    if (cpus.empty()) cpus.push_back(0);
    if (cpus.size() == 1) cpus.push_back(cpus[0]);        // one CPU only: run 3 cannot move the spinner away
    return cpus;
}

// pins the calling thread (episode 5); a refusal is reported and the run continues unpinned
static void pin_self(int cpu) {
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        std::cout << "  sched_setaffinity(cpu " << cpu << ") refused: " << std::strerror(errno) << "\n";
#else
    (void)cpu;                                            // no portable pinning: the fallback runs unpinned
#endif
}

// a thread that never sleeps, pinned to one CPU, stopped and joined by the destructor
struct Spinner {
    std::atomic<bool> stop{false};
    std::thread worker;
    explicit Spinner(int cpu) : worker([this, cpu] {
        pin_self(cpu);
        while (!stop.load(std::memory_order_relaxed)) sink(1);
    }) {}
    ~Spinner() {
        stop.store(true);
        worker.join();
    }
    Spinner(const Spinner&) = delete;
    Spinner& operator=(const Spinner&) = delete;
};

static long long percentile(const std::vector<long long>& sorted, double p) {
    double pos = p / 100.0 * static_cast<double>(sorted.size());
    auto rank = static_cast<std::size_t>(pos);
    return sorted[std::min(rank, sorted.size() - 1)];
}

static void report(const char* name, std::vector<long long> late) {
    std::sort(late.begin(), late.end());
    std::cout << "  " << std::left << std::setw(22) << name << std::right
              << " p50 " << std::setw(6) << percentile(late, 50.0) / 1000 << " us"
              << "  p99 " << std::setw(6) << percentile(late, 99.0) / 1000 << " us"
              << "  p99.9 " << std::setw(6) << percentile(late, 99.9) / 1000 << " us"
              << "  max " << std::setw(6) << late.back() / 1000 << " us\n";
}

int main() {
#if !defined(__linux__)
    std::cout << "this sample needs Linux: sched_setaffinity pins the runs; here all three run unpinned\n";
#endif
    std::vector<int> cpus = allowed_cpus();
    std::cout << "three runs of " << kSamples << " wakeups, this machine: our CPU " << cpus[0]
              << ", the other CPU " << cpus[1] << "\n";

    // same loop, three runs: only the neighbour changes
    pin_self(cpus[0]);                      // sched_setaffinity, episode 5
    report("alone", measure(kSamples));
    {
        Spinner noisy(cpus[0]);             // competes for our CPU
        report("spinner on our CPU", measure(kSamples));
    }                                       // joined here
    Spinner apart(cpus[1]);                 // same load, another CPU
    report("spinner pinned apart", measure(kSamples));

    std::cout << "read the tail columns: the median often stays close, p99.9 and max show the neighbour\n";
    std::cout << "numbers from WSL or a virtual machine are noisy: repeat on real Linux hardware\n";
    return 0;
}
