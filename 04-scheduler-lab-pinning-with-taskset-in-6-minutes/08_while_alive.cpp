// Pinning with taskset in 6 Minutes: One Process, One Core - slide 8: thank you
// Build: make 08_while_alive
#include <iostream>
#include <queue>
#include <string>

// The imaginary tomjnet.h of the slide, made real enough to run.
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_lab_topic() {
    static const char* topics[] = {"CPU affinity in code: sched_setaffinity",
                                   "priorities, real time classes and preemption",
                                   "core isolation: isolcpus and nohz_full"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> lab;
    while (alive()) {
        lab.push(next_lab_topic());      // next: sched_setaffinity
        lab.push(viewer_request());      // the comments are the backlog
        subscribe();                     // pinned to this channel
    }

    std::cout << "the lab queue, first in, first out:\n";
    while (!lab.empty()) {
        std::cout << "  " << lab.front().name << '\n';
        lab.pop();
    }
    return 0;
}
