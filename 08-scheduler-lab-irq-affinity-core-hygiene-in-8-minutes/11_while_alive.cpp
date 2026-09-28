// IRQ Affinity and Core Hygiene in 8 Minutes - slide 11: thank you
// Build: make 11_while_alive
#include <iostream>
#include <queue>
#include <string>

// The imaginary "tomjnet.h" of the slide: three episodes, then the demo ends.
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }
static Topic next_scheduler_topic() {
    static const char* topics[] = {"Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails",
                                   "A Repeatable Low Latency Core Recipe in 10 Minutes",
                                   "next lab: Interrupts and Timers"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> todo;
    while (alive()) {
        todo.push(next_scheduler_topic());  // next: measure the jitter
        todo.push(viewer_request());        // on a housekeeping core
        subscribe();                        // no root needed
    }

    std::cout << "queued topics, first in first out:\n";
    while (!todo.empty()) {
        std::cout << "  " << todo.front().name << "\n";
        todo.pop();
    }
    return 0;
}
