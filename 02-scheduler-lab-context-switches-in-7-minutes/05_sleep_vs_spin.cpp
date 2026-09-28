// Context Switches in 7 Minutes: The Hidden Cost - slide 5: sleep loop against spin loop
// Build: make 05_sleep_vs_spin
//
// A loop that sleeps gives the core away every time (voluntary switches); a loop that spins never
// does, so only the scheduler can take the core from it (involuntary switches). The counters come
// from /proc/thread-self/status; without /proc (Windows) they stay at zero and the loops still run.
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono_literals;

static volatile std::uint64_t g_sink = 0;
static void sink(std::uint64_t x) { g_sink = g_sink + x; }   // keeps the result alive: the loop cannot be deleted

struct Switches { long vol = 0; long invol = 0; };
static Switches read_switches() {       // this thread: one open, one scan
    Switches s;
    std::ifstream in("/proc/thread-self/status");
    for (std::string key; in >> key; )
        if (key == "voluntary_ctxt_switches:") in >> s.vol;
        else if (key == "nonvoluntary_ctxt_switches:") in >> s.invol;
    return s;
}

int main() {
    const bool has_proc = static_cast<bool>(std::ifstream("/proc/thread-self/status"));
    if (!has_proc)
        std::cout << "this sample needs Linux: it would show about 200 voluntary switches for the\n"
                     "sleep loop and none for the spin loop; the loops run anyway, the counters read 0\n";
    (void)read_switches();               // warm up: the first read of a /proc file allocates

    // sleeping gives the core away: one voluntary switch per sleep
    Switches a = read_switches();
    for (int i = 0; i < 200; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Switches b = read_switches();        // vol: about 200 more

    // spinning never blocks: only the scheduler can take the core
    auto end = std::chrono::steady_clock::now() + 300ms;
    std::uint64_t x = 0;
    while (std::chrono::steady_clock::now() < end) ++x;
    Switches c = read_switches();        // vol flat, invol: a few
    sink(x);

    std::cout << "sleep loop, 200 x 1 ms: voluntary +" << (b.vol - a.vol)
              << ", involuntary +" << (b.invol - a.invol) << '\n';
    std::cout << "spin loop, 300 ms:      voluntary +" << (c.vol - b.vol)
              << ", involuntary +" << (c.invol - b.invol) << "  (" << x << " iterations)\n";
    std::cout << "this machine: involuntary switches appear only when another task wants this core\n";
    return 0;
}
