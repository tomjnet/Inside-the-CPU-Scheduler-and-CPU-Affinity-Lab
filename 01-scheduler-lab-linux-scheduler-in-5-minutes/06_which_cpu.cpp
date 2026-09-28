// The Linux Scheduler in 5 Minutes: Who Runs Next? - slide 6: which cpu am i on? sched_getcpu
// Build: make 06_which_cpu
#include <chrono>
#include <cstdio>
#include <thread>

#if defined(__linux__)
#include <sched.h>
#endif

int main() {
    using namespace std::chrono_literals;

    // portable part: how many logical CPUs the scheduler can choose from
    unsigned cpus = std::thread::hardware_concurrency();
    std::printf("logical CPUs: %u, so %u run queues\n", cpus, cpus);

#if defined(__linux__)
    // which CPU runs this thread? ask 10 times, 100 ms apart
    int last = -1, moves = 0;
    for (int i = 0; i < 10; ++i) {
        int cpu = sched_getcpu();        // a snapshot, no system call
        if (last >= 0 && cpu != last) ++moves;  // migrated since last look
        std::printf("sample %d: CPU %d\n", i, cpu);
        last = cpu;
        std::this_thread::sleep_for(100ms);     // sleep, wakeup, new pick
    }

    std::printf("the CPU changed %d times in 10 samples on this machine\n", moves);
    std::printf("each sample is a sleep, a wakeup and a new placement decision\n");
    std::printf("try again while another terminal runs something heavy\n");
#else
    std::printf("this sample needs Linux: it would print the CPU of this thread\n");
    std::printf("10 times, 100 ms apart, and count how often the scheduler moved it\n");
#endif
    return 0;
}
