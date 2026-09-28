// A Repeatable Low Latency Core Recipe in 10 Minutes - slide 9: measure again: the before and after report
// Build: make 09_report
// Takes about 10 seconds: two runs of 5000 wakeups, one per millisecond.
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <vector>

#if defined(__linux__)
#include <sched.h>
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

// The probe of slide 4 (04_baseline.cpp).
static std::vector<long long> measure(std::size_t n) {
    std::vector<long long> late(n);
    timespec next = now_ts();
    for (auto& ns : late) {
        add_ns(next, 1'000'000);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);
        ns = now_ns() - to_ns(next);
    }
    std::sort(late.begin(), late.end());
    return late;
}

// Stage 2 stand-in: the second CPU this process is allowed on; use the CPU you picked from the topology.
static int pick_app_cpu() {
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof allowed, &allowed) != 0) return 0;
    int first = -1;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (!CPU_ISSET(cpu, &allowed)) continue;
        if (first >= 0) return cpu;
        first = cpu;
    }
    return first < 0 ? 0 : first;
}

static void pin_to(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof set, &set) != 0) std::perror("sched_setaffinity");
    std::printf("the one change: pinned to CPU %d, now running on CPU %d\n", cpu, sched_getcpu());
}
#else
#include <chrono>
#include <thread>

// Portable stand-in for the probe: the same absolute deadlines with std::chrono (coarse timer on Windows).
static std::vector<long long> measure(std::size_t n) {
    std::vector<long long> late(n);
    auto next = std::chrono::steady_clock::now();
    for (auto& ns : late) {
        next += std::chrono::milliseconds(1);
        std::this_thread::sleep_until(next);
        ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - next).count();
    }
    std::sort(late.begin(), late.end());
    return late;
}
static int pick_app_cpu() { return 0; }
static void pin_to(int cpu) {
    std::printf("this sample needs Linux: pin_to(%d) would call sched_setaffinity; here both rows are baselines\n", cpu);
}
#endif

// Measure again: same probe, same n, one change between the rows
void report(const char* stage, const std::vector<long long>& s) {
    auto at = [&](std::size_t permille) {  // s is sorted: O(1) each
        return s[(s.size() - 1) * permille / 1000] / 1000;  // ns to us
    };
    std::printf("%-9s p50 %5lld p99 %5lld p99.9 %5lld max %5lld us\n",
                stage, at(500), at(990), at(999), at(1000));
}

int main() {
    const int app_cpu = pick_app_cpu();
    std::printf("two runs of 5000 wakeups, one per millisecond: about 10 seconds\n");

    auto before = measure(5000);               // stage 1: the baseline
    pin_to(app_cpu);                           // exactly one change
    auto after = measure(5000);                // same probe, same n
    report("baseline", before), report("pinned", after);

    std::printf("numbers of this machine, this load, this run: on an idle machine the pin may change little\n");
    std::printf("add one row per stage (memory, priority, isolation, IRQs) and keep a stage only if the tail moved\n");
    return 0;
}
