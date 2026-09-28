// Context Switches in 7 Minutes: The Hidden Cost - slide 4: voluntary against involuntary
// Build: make 04_read_counters
//
// Reads the two context switch counters Linux keeps for every thread, and proves the detail of the
// slide: /proc/self/status is the main thread, /proc/thread-self/status is whichever thread asks.
// The file is plain text, so the reader is portable C++; without /proc the counters stay at zero.
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

struct Switches { long vol = 0; long invol = 0; };
Switches read_switches() {       // this thread: one open, one scan
    Switches s;
    std::ifstream in("/proc/thread-self/status");
    for (std::string key; in >> key; )
        if (key == "voluntary_ctxt_switches:") in >> s.vol;
        else if (key == "nonvoluntary_ctxt_switches:") in >> s.invol;
    return s;
}

// The same scan on any status file, to compare /proc/self with /proc/thread-self.
static Switches read_switches_from(const char* path) {
    Switches s;
    std::ifstream in(path);
    for (std::string key; in >> key; )
        if (key == "voluntary_ctxt_switches:") in >> s.vol;
        else if (key == "nonvoluntary_ctxt_switches:") in >> s.invol;
    return s;
}

static void print(const char* who, const Switches& s) {
    std::cout << "  " << who << ": voluntary " << s.vol << ", involuntary " << s.invol << '\n';
}

int main() {
    if (!std::ifstream("/proc/thread-self/status")) {
        std::cout << "this sample needs Linux: it would print voluntary_ctxt_switches and\n"
                     "nonvoluntary_ctxt_switches of the main thread and of a worker thread\n";
        print("without /proc every counter reads", read_switches());
        return 0;
    }

    (void)read_switches();                     // warm up: the first read of a /proc file allocates
    const Switches main_before = read_switches();
    print("main thread, baseline", main_before);

    // A worker that blocks 50 times. Its switches belong to the worker, not to the main thread.
    Switches worker_delta;
    Switches self_seen_by_worker;
    std::thread worker([&] {
        (void)read_switches();
        const Switches a = read_switches();
        for (int i = 0; i < 50; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const Switches b = read_switches();
        worker_delta = Switches{b.vol - a.vol, b.invol - a.invol};
        self_seen_by_worker = read_switches_from("/proc/self/status");   // still the main thread
    });
    worker.join();                             // one voluntary switch of the main thread: it waits here

    const Switches main_after = read_switches();
    print("worker, 50 sleeps of 1 ms (its own delta)", worker_delta);
    print("/proc/self/status read inside the worker", self_seen_by_worker);
    print("main thread, after the join", main_after);
    std::cout << "the worker blocked about 50 times; the main thread only moved by "
              << (main_after.vol - main_before.vol) << " voluntary switch(es)\n";
    return 0;
}
