// Core Isolation in 9 Minutes: isolcpus and nohz_full - slide 8: cpu lists: who is housekeeping
// Build: make 08_cpu_list
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

static bool is_digit(char ch) { return ch >= '0' && ch <= '9'; }

// Reads one decimal number at text[i] and advances i; false when there is no digit there.
static bool read_number(const std::string& text, std::size_t& i, int& out) {
    if (i >= text.size() || !is_digit(text[i])) return false;
    out = 0;
    while (i < text.size() && is_digit(text[i]) && out < 100000) out = out * 10 + (text[i++] - '0');
    return true;
}

// The kernel CPU list format: "2-3,6" becomes {2, 3, 6}. Anything that is not a digit, a dash or a comma
// ends the parse, so an empty line or "(null)" gives an empty list instead of an exception.
static std::vector<int> parse_cpu_list(const std::string& text) {
    std::vector<int> cpus;
    std::size_t i = 0;
    int lo = 0;
    while (read_number(text, i, lo)) {
        int hi = lo;
        if (i < text.size() && text[i] == '-') {
            ++i;
            if (!read_number(text, i, hi)) break;
        }
        for (int c = lo; c <= hi && c < 4096; ++c) cpus.push_back(c);
        if (i >= text.size() || text[i] != ',') break;
        ++i;
    }
    return cpus;
}

static void print(const char* label, const std::vector<int>& cpus) {
    std::cout << label << ":";
    for (int cpu : cpus) std::cout << " " << cpu;
    if (cpus.empty()) std::cout << " (none)";
    std::cout << "\n";
}

int main() {
    // parse_cpu_list is the episode 4 parser: "2-3,6" becomes {2, 3, 6}.
    // Housekeeping = online minus isolated: arithmetic, no system call.
    std::vector<int> online   = parse_cpu_list("0-7");
    std::vector<int> isolated = parse_cpu_list("2-3,6");
    std::vector<int> housekeeping;
    for (int cpu : online)
        if (std::count(isolated.begin(), isolated.end(), cpu) == 0)
            housekeeping.push_back(cpu);             // 0 1 4 5 7
    // Rule of the lab: CPU 0 always stays a housekeeping CPU.
    bool cpu0_ok =
        std::count(housekeeping.begin(), housekeeping.end(), 0) == 1;

    print("online      ", online);
    print("isolated    ", isolated);
    print("housekeeping", housekeeping);
    std::cout << "CPU 0 is housekeeping: " << (cpu0_ok ? "yes" : "NO, fix the boot line") << "\n";
    std::cout << "left for everything else: " << housekeeping.size() << " of " << online.size() << " CPUs\n";

    // Edge cases of the files under /sys: an empty line, a single CPU, a boot line that isolates CPU 0.
    print("empty line  ", parse_cpu_list(""));
    print("single \"5\"  ", parse_cpu_list("5"));
    std::vector<int> bad = parse_cpu_list("0-3");
    std::cout << "isolcpus=0-3 breaks the rule, CPU 0 isolated: "
              << (std::count(bad.begin(), bad.end(), 0) == 1 ? "yes" : "no") << "\n";
    return cpu0_ok ? 0 : 1;
}
