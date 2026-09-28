// CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 4: sched_setaffinity: pin the calling thread
// Build: make 04_pin_self
#include <iostream>

#if defined(__linux__)
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sched.h>
#include <vector>

bool pin_self(int target) {                  // target: an allowed CPU
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(target, &one);
    // pid 0 means the calling thread: one system call, at startup
    if (sched_setaffinity(0, sizeof one, &one) != 0) {
        // EINVAL: no allowed CPU in the mask; ESRCH: no such thread
        fprintf(stderr, "pin failed: %s\n", strerror(errno));
        return false;                        // keep running, unpinned
    }
    return sched_getcpu() == target;         // migrated before return
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
    std::cout << "allowed CPUs: " << cpus.size() << ", running on CPU " << sched_getcpu() << "\n";

    // a CPU this machine does not have: the call is refused and the program keeps running
    std::cout << "pin to CPU 1023 (expect EINVAL on stderr): ";
    std::cout.flush();
    bool refused = !pin_self(CPU_SETSIZE - 1);
    std::cout << (refused ? "refused, still unpinned" : "accepted: this machine is very large") << "\n";

    // the real pin: the last CPU of the allowed set, never a fixed number
    int target = cpus.back();
    bool ok = pin_self(target);
    std::cout << "pin to CPU " << target << ": " << (ok ? "ok" : "failed") << ", sched_getcpu() = " << sched_getcpu() << "\n";
    std::cout << "allowed CPUs now: " << allowed_cpus().size() << "\n";
#else
    std::cout << "this sample needs Linux: sched_setaffinity(0, ...) pins the calling thread, sched_getcpu() confirms it\n";
#endif
    return 0;
}
