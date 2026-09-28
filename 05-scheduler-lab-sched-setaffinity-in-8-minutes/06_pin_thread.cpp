// CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 6: pin a std::thread through native_handle
// Build: make 06_pin_thread
#include <iostream>

#if defined(__linux__)
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <vector>

static std::atomic<int> first_cpu{-1};       // where the worker ran before the pin reached it
static std::atomic<int> final_cpu{-1};       // where it ran once its mask held one CPU
static std::atomic<int> final_count{0};

static int allowed_count() {
    cpu_set_t mine;
    CPU_ZERO(&mine);
    if (sched_getaffinity(0, sizeof mine, &mine) != 0) return 0;
    return CPU_COUNT(&mine);
}

// the worker knows nothing about affinity: it spins until somebody narrows its mask, one second at most
static void work() {
    first_cpu.store(sched_getcpu());
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (allowed_count() != 1 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    final_count.store(allowed_count());
    final_cpu.store(sched_getcpu());
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
#endif

int main() {
#if defined(__linux__)
    std::vector<int> cpus = allowed_cpus();
    if (cpus.empty()) {
        std::cout << "sched_getaffinity gave no CPUs: nothing to pin to\n";
        return 0;
    }
    int wanted = cpus[1 % cpus.size()];

    // pin another thread: std::thread hands out its pthread_t
    std::thread worker(work);
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpus[1 % cpus.size()], &set);        // second allowed CPU
    // returns the error number itself and leaves errno alone
    int err = pthread_setaffinity_np(worker.native_handle(),
                                     sizeof set, &set);
    if (err != 0) fprintf(stderr, "pin: %s\n", strerror(err));
    // the worker started before the pin: it may have run elsewhere
    worker.join();

    std::cout << "main thread still has " << allowed_count() << " allowed CPUs: the mask belongs to a thread\n";
    std::cout << "worker started on CPU " << first_cpu.load() << " (before the pin arrived)\n";
    std::cout << "worker ended on CPU " << final_cpu.load() << " with " << final_count.load()
              << " allowed CPU, wanted CPU " << wanted << ": " << (final_cpu.load() == wanted ? "ok" : "not pinned") << "\n";
    std::cout << "cleaner: pass the CPU to the worker and let its first line call sched_setaffinity(0, ...)\n";
#else
    std::cout << "this sample needs Linux: pthread_setaffinity_np on the native_handle of a std::thread\n";
#endif
    return 0;
}
