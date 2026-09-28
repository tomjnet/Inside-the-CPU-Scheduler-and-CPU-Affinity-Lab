// IRQ Affinity and Core Hygiene in 8 Minutes - slide 7: parse one row of /proc/interrupts in c++
// Build: make 07_parse_row
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

struct Row { std::string label; std::vector<std::uint64_t> cpu; };

// "25:  0  0  34759  0  PCI-MSIX  eth0-rx-1" becomes one Row.
// Cost: string work only, no system call; the read was one open.
Row parse_row(const std::string& line, std::size_t ncpu) {
    std::istringstream ls(line);
    Row r;
    ls >> r.label;                          // "25:" or "LOC:"
    for (std::uint64_t n; r.cpu.size() < ncpu && ls >> n;)
        r.cpu.push_back(n);                 // stops at the chip name
    return r;                               // ERR, MIS: fewer than ncpu
}

// The header line is "CPU0 CPU1 CPU2": one word per online CPU.
static std::size_t count_cpus(const std::string& header) {
    std::istringstream hs(header);
    std::size_t n = 0;
    for (std::string word; hs >> word;) ++n;
    return n;
}

// Parses every line after the header, prints the kept rows and counts the dropped ones.
static void report(std::istream& in, std::size_t max_rows) {
    std::string line;
    std::getline(in, line);
    const std::size_t ncpu = count_cpus(line);
    std::cout << "  header: " << ncpu << " online CPUs\n";
    std::size_t kept = 0, dropped = 0;
    while (std::getline(in, line)) {
        const Row r = parse_row(line, ncpu);
        if (r.cpu.size() != ncpu) {         // the caller drops the short rows
            std::cout << "  dropped " << r.label << " (" << r.cpu.size() << " of " << ncpu << " counters)\n";
            ++dropped;
            continue;
        }
        ++kept;
        if (kept > max_rows) continue;
        const std::uint64_t total = std::accumulate(r.cpu.begin(), r.cpu.end(), std::uint64_t{0});
        const auto top = std::max_element(r.cpu.begin(), r.cpu.end());
        std::cout << "  " << r.label << " total " << total << ", busiest CPU" << (top - r.cpu.begin())
                  << " with " << *top << "\n";
    }
    if (kept > max_rows) std::cout << "  (" << (kept - max_rows) << " more rows parsed, not printed)\n";
    std::cout << "  rows kept: " << kept << ", rows dropped: " << dropped << "\n";
}

int main() {
    // A typical file, so the parser shows its work on every operating system.
    const std::string typical =
        "           CPU0       CPU1       CPU2       CPU3\n"
        " 24:          0          0     918204          0  PCI-MSIX  eth0-rx-0\n"
        " 25:          0          0          0     877512  PCI-MSIX  eth0-rx-1\n"
        " 31:      40211          0          0          0  PCI-MSIX  nvme0q1\n"
        "LOC:     902113     899120     911822     915377  Local timer interrupts\n"
        "RES:      37226      39409      51080      35507  Rescheduling interrupts\n"
        "ERR:          0\n"
        "MIS:          0\n";
    std::cout << "typical /proc/interrupts (built in):\n";
    std::istringstream sample(typical);
    report(sample, 16);

    // The real file: a plain read, no root. It exists on Linux only (WSL included).
    std::ifstream real("/proc/interrupts");
    if (!real) {
        std::cout << "this part needs Linux: it would parse the real /proc/interrupts of this machine\n";
        return 0;
    }
    std::cout << "\n/proc/interrupts of this machine (counts since boot):\n";
    report(real, 12);
    return 0;
}
