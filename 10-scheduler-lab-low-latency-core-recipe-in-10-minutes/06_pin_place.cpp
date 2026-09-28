// A Repeatable Low Latency Core Recipe in 10 Minutes - slide 6: stage 3 and 4: pin, then place memory
// Build: make 06_pin_place
#include <cstddef>
#include <cstdio>
#include <memory>

static constexpr std::size_t kBytes = std::size_t{16} << 20;   // 16 MiB of "ring buffer" for the worker

#if defined(__linux__)
#include <sched.h>
#include <sys/mman.h>

// Stage 2 stand-in: the second CPU this process is allowed on (the first one is usually housekeeping).
// On your machine, put here the CPU you picked from lscpu -e and thread_siblings_list.
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
#endif

int main() {
#if defined(__linux__)
    const int app_cpu = pick_app_cpu();
    std::printf("before the pin: running on CPU %d, the application CPU will be %d\n", sched_getcpu(), app_cpu);

    // Stage 3: pin first. Stage 4: then allocate, touch and lock memory
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(app_cpu, &set);                    // app_cpu chosen in stage 2
    if (sched_setaffinity(0, sizeof set, &set) != 0)
        std::perror("sched_setaffinity");      // check, never assume
    auto ring = std::make_unique_for_overwrite<char[]>(kBytes);
    for (std::size_t i = 0; i < kBytes; i += 4096)
        ring[i] = 1;                           // first touch: local node
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        std::perror("mlockall");               // else: no page faults later
    std::printf("running on CPU %d\n", sched_getcpu());

    std::size_t touched = 0;
    for (std::size_t i = 0; i < kBytes; i += 4096) touched += static_cast<std::size_t>(ring[i]);
    std::printf("%zu pages of 4 KiB touched from the pinned core: first touch places them on its NUMA node\n", touched);
    std::printf("an mlockall error above means a small RLIMIT_MEMLOCK (ulimit -l): raise it, or run as root\n");
    munlockall();                              // rollback: the sample leaves nothing locked
#else
    auto ring = std::make_unique_for_overwrite<char[]>(kBytes);
    std::size_t touched = 0;
    for (std::size_t i = 0; i < kBytes; i += 4096) {
        ring[i] = 1;                           // the portable half: allocate without touching, then touch each page
        touched += static_cast<std::size_t>(ring[i]);
    }
    std::printf("%zu pages of 4 KiB touched\n", touched);
    std::printf("this sample needs Linux: it pins the thread with sched_setaffinity, touches the memory from the\n"
                "pinned core so first touch places it on the local NUMA node, and locks it with mlockall\n");
#endif
    return 0;
}
