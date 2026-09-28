// Pinning with taskset in 6 Minutes: One Process, One Core - slide 3: cpu lists and hex masks, in code
// Build: make 03_list_to_mask
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

// "0,2-3" -> bits 0, 2 and 3 -> 0xd: one bit per logical CPU
std::uint64_t list_to_mask(const std::string& list) {
    std::uint64_t mask = 0;
    std::istringstream in(list);
    for (std::string part; std::getline(in, part, ',');) {
        std::size_t dash = part.find('-');    // "2-3" or just "5"
        int lo = std::stoi(part), hi = lo;    // stoi stops at '-'
        if (dash != part.npos) hi = std::stoi(part.substr(dash + 1));
        for (int cpu = lo; cpu <= hi; ++cpu) mask |= 1ull << cpu;
    }
    return mask;                              // this model: 64 CPUs
}

// The way back, as the slide promises: 0xd -> "0,2-3". Consecutive set bits become one range.
static std::string mask_to_list(std::uint64_t mask) {
    std::string list;
    for (int cpu = 0; cpu < 64; ++cpu) {
        if (((mask >> cpu) & 1u) == 0) continue;
        int last = cpu;
        while (last + 1 < 64 && ((mask >> (last + 1)) & 1u) != 0) ++last;
        if (!list.empty()) list += ',';
        list += std::to_string(cpu);
        if (last > cpu) list += '-' + std::to_string(last);
        cpu = last;
    }
    return list;
}

int main() {
    const char* lists[] = {"0-7", "0,2-3", "2", "1", "0,4", "2,4,6", "0-3,8-11", "63"};
    int bad = 0;
    std::cout << "list          hex mask            back to a list\n";
    for (const char* text : lists) {
        std::uint64_t mask = list_to_mask(text);
        std::string back = mask_to_list(mask);
        std::ostringstream hex;
        hex << "0x" << std::hex << mask;
        std::cout << std::left << std::setw(14) << text << std::setw(20) << hex.str() << back << '\n';
        if (back != text) ++bad;
    }

    // the classic mistake of slide 4: without -c, taskset reads a hex mask
    std::cout << "\ntaskset 2 ./app     is mask 0x2: CPU " << mask_to_list(0x2) << ", not CPU 2\n";
    std::cout << "taskset -c 2 ./app  is mask 0x" << std::hex << list_to_mask("2") << std::dec << ": CPU 2\n";
    std::cout << "taskset 0xd ./app   is the list " << mask_to_list(0xd) << '\n';

    if (bad != 0) {
        std::cout << bad << " round trips differ\n";
        return 1;
    }
    std::cout << "every list survived the round trip\n";
    return 0;
}
