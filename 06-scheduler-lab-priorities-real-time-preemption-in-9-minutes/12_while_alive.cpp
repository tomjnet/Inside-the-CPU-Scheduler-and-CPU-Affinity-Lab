// Priorities, Real Time Classes and Preemption in 9 Minutes - slide 12: thank you
// Build: make 12_while_alive
#include <iostream>
#include <queue>
#include <string>

// The imaginary tomjnet.h of the slide.
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_scheduler_topic() {
    static const char* topics[] = {"core isolation: isolcpus and nohz_full", "IRQ affinity and core hygiene",
                                   "measure scheduler jitter"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> run_queue;
    while (alive()) {
        run_queue.push(next_scheduler_topic()); // core isolation
        run_queue.push(viewer_request());       // no time slice
        subscribe();                            // lifetime benefit
    }

    std::cout << "run queue, first in first out:\n";
    while (!run_queue.empty()) {
        std::cout << "  " << run_queue.front().name << '\n';
        run_queue.pop();
    }
    return 0;
}
