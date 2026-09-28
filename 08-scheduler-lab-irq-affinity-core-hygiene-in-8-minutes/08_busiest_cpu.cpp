// IRQ Affinity and Core Hygiene in 8 Minutes - slide 8: one second of deltas: the busiest cpu
// Build: make 08_busiest_cpu
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using Counters = std::vector<std::uint64_t>;
using Snapshot = std::map<std::string, Counters>;       // "25:" or "LOC:" -> one counter per CPU

// Two typical files one second apart, used when /proc/interrupts does not exist (Windows, macOS).
static const char* const TYPICAL[2] = {
    "           CPU0       CPU1       CPU2       CPU3\n"
    " 24:          0          0     918204          0  PCI-MSIX  eth0-rx-0\n"
    " 25:          0          0          0     877512  PCI-MSIX  eth0-rx-1\n"
    " 31:      40211          0          0          0  PCI-MSIX  nvme0q1\n"
    "LOC:     902113     899120     911822     915377  Local timer interrupts\n"
    "RES:      37226      39409      51080      35507  Rescheduling interrupts\n"
    "ERR:          0\n",
    "           CPU0       CPU1       CPU2       CPU3\n"
    " 24:          0          0     930516          0  PCI-MSIX  eth0-rx-0\n"
    " 25:          0          0          0     886040  PCI-MSIX  eth0-rx-1\n"
    " 31:      40563          0          0          0  PCI-MSIX  nvme0q1\n"
    "LOC:     902363     899370     912072     915627  Local timer interrupts\n"
    "RES:      37241      39420      51102      35519  Rescheduling interrupts\n"
    "ERR:          0\n"};

static bool g_builtin = false;                          // true when the typical text was used

// The whole file as text: one open, one read. The built in text when the file is missing.
static std::string read_all(const std::string& path, int which) {
    std::ifstream f(path);
    if (!f) {
        g_builtin = true;
        return TYPICAL[which % 2];
    }
    std::ostringstream all;
    all << f.rdbuf();
    return all.str();
}

// The header line is "CPU0 CPU1 CPU2": one word per online CPU.
static std::size_t cpu_count(const std::string& path) {
    std::istringstream in(read_all(path, 0));
    std::string header;
    std::getline(in, header);
    std::istringstream hs(header);
    std::size_t n = 0;
    for (std::string word; hs >> word;) ++n;
    return n;
}

// The parser of slide 7, storing the rows by label. Short rows (ERR, MIS) are dropped.
static Snapshot snapshot(const std::string& path) {
    static int reads = 0;
    std::istringstream in(read_all(path, reads++));
    std::string line;
    std::getline(in, line);
    std::istringstream hs(line);
    std::size_t ncpu = 0;
    for (std::string word; hs >> word;) ++ncpu;
    Snapshot rows;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string label;
        ls >> label;
        Counters cpu;
        for (std::uint64_t n; cpu.size() < ncpu && ls >> n;) cpu.push_back(n);
        if (cpu.size() == ncpu && ncpu > 0) rows[label] = std::move(cpu);
    }
    return rows;
}

// Numbered rows are device interrupts, the ones an affinity file can move.
static bool is_device(const std::string& label) {
    return !label.empty() && label[0] >= '0' && label[0] <= '9';
}

// now minus before for one row; zeros when the row is new or changed its width (CPU hotplug).
static Counters delta(const Snapshot& before, const std::string& label, const Counters& now) {
    Counters d(now.size(), 0);
    const auto was = before.find(label);
    if (was == before.end() || was->second.size() != now.size()) return d;
    for (std::size_t c = 0; c < now.size(); ++c)
        if (now[c] >= was->second[c]) d[c] = now[c] - was->second[c];
    return d;
}

int main() {
    // Also the warm up read: the first read of a /proc file can fault in pages and allocate.
    const std::size_t ncpu = cpu_count("/proc/interrupts");
    if (ncpu == 0) {
        std::cout << "no CPU columns found in /proc/interrupts: nothing to measure\n";
        return 0;
    }

    // Two snapshots one second apart: the delta is the interrupt rate.
    // Reading needs no root; only moving an IRQ does.
    const auto before = snapshot("/proc/interrupts");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const auto after = snapshot("/proc/interrupts");
    std::vector<std::uint64_t> device(ncpu), local(ncpu);
    for (const auto& [label, now] : after) {
        auto& sum = is_device(label) ? device : local;  // "25:" or "LOC:"
        const auto d = delta(before, label, now);       // zeros if new
        for (std::size_t c = 0; c < ncpu; ++c) sum[c] += d[c];
    }
    const auto busiest = std::max_element(device.begin(), device.end());

    std::cout << (g_builtin ? "source: built in typical snapshots (this sample needs Linux to read the real "
                              "/proc/interrupts)\n"
                            : "source: /proc/interrupts of this machine\n");
    std::cout << "interrupts in one second, " << ncpu << " online CPUs, " << after.size() << " rows:\n";
    std::cout << "  CPU     device      local\n";
    for (std::size_t c = 0; c < ncpu; ++c)
        std::cout << "  " << std::setw(3) << c << std::setw(11) << device[c] << std::setw(11) << local[c] << "\n";

    if (*busiest == 0) {
        std::cout << "no device interrupt in this second: run it again during network or disk work\n";
    } else {
        std::cout << "busiest CPU for device interrupts: CPU" << (busiest - device.begin()) << " with " << *busiest
                  << " in one second\n";
    }
    const auto timer = std::max_element(local.begin(), local.end());
    std::cout << "busiest CPU for local interrupts (timer, IPIs): CPU" << (timer - local.begin()) << " with "
              << *timer << ", no affinity file moves these\n";

    // The three sources that fired most: the rows to move first.
    std::vector<std::pair<std::uint64_t, std::string>> top;
    for (const auto& [label, now] : after) {
        const auto d = delta(before, label, now);
        top.emplace_back(std::accumulate(d.begin(), d.end(), std::uint64_t{0}), label);
    }
    std::sort(top.rbegin(), top.rend());
    std::cout << "top sources:";
    for (std::size_t i = 0; i < top.size() && i < 3; ++i) std::cout << "  " << top[i].second << " " << top[i].first;
    std::cout << "\nhygiene check: after moving the IRQs, the device column of your application cores reads 0\n";
    return 0;
}
