// Core Isolation in 9 Minutes: isolcpus and nohz_full - slide 7: read the same files in c++
// Build: make 07_read_isolation
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>

// True when the boot line holds "<name>=" as the start of a parameter.
static bool has_parameter(const std::string& cmdline, const std::string& name) {
    std::size_t at = cmdline.find(name + "=");
    while (at != std::string::npos) {
        if (at == 0 || cmdline[at - 1] == ' ') return true;
        at = cmdline.find(name + "=", at + 1);
    }
    return false;
}

int main() {
#if !defined(__linux__)
    std::cout << "this sample needs Linux: it would print /proc/cmdline and the isolated and nohz_full CPU lists\n"
              << "the loop still runs, so every file reads as (not available):\n";
#endif
    // Isolation state is three text files: open, read, close, once each.
    const char* files[] = {
        "/proc/cmdline",                         // the boot line itself
        "/sys/devices/system/cpu/isolated",      // what isolcpus did
        "/sys/devices/system/cpu/nohz_full",     // CPUs without the tick
    };
    for (const char* path : files) {
        std::ifstream in(path);
        std::string line = "(not available)";    // no file: no feature
        if (in && std::getline(in, line) && line.empty()) line = "(none)";
        std::cout << path << ": " << line << "\n";
    }

    // The same boot line, searched for the three parameters of the episode.
    std::ifstream in("/proc/cmdline");
    std::string cmdline;
    if (in) std::getline(in, cmdline);
    const char* names[] = {"isolcpus", "nohz_full", "rcu_nocbs"};
    std::cout << "\nboot parameters of this machine:\n";
    for (const char* name : names)
        std::cout << "  " << name << ": " << (has_parameter(cmdline, name) ? "present" : "absent") << "\n";

    // The search on a typical lab boot line, so the output teaches on any machine.
    const std::string typical = "BOOT_IMAGE=/vmlinuz ro quiet splash isolcpus=2-3 nohz_full=2-3 rcu_nocbs=2-3";
    std::cout << "typical lab boot line: " << typical << "\n";
    for (const char* name : names)
        std::cout << "  " << name << ": " << (has_parameter(typical, name) ? "present" : "absent") << "\n";
    return 0;
}
