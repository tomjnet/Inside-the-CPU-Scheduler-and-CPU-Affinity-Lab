// Core Isolation in 9 Minutes: isolcpus and nohz_full - slide 12: thank you
// Build: make 12_while_alive
#include <iostream>
#include <queue>
#include <string>

// The imaginary tomjnet.h of the slide.
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_lab_topic() {
    static const char* topics[] = {"IRQ affinity and core hygiene", "measure scheduler jitter",
                                   "a repeatable low latency core recipe"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> lab;
    while (alive()) {
        lab.push(next_lab_topic());      // IRQ affinity is next
        lab.push(viewer_request());      // housekeeping CPU work
        subscribe();                     // lifetime benefit
    }

    std::cout << "next in the Scheduler and CPU Affinity Lab:\n";
    while (!lab.empty()) {
        std::cout << "  " << lab.front().name << "\n";
        lab.pop();
    }
    return 0;
}
