// Context Switches in 7 Minutes: The Hidden Cost - slide 9: thank you
// Build: make 09_while_alive
#include <iostream>
#include <queue>
#include <string>

// The imaginary tomjnet.h of the slide.
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_scheduler_topic() {
    static const char* topics[] = {"CPU migration and cache warmth", "pinning with taskset",
                                   "CPU affinity in code: sched_setaffinity"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> run_queue;
    while (alive()) {
        run_queue.push(next_scheduler_topic());  // CPU migration
        run_queue.push(viewer_request());        // wakes up next
        subscribe();                             // never preempted
    }

    std::cout << "run queue, in the order it will be scheduled:\n";
    while (!run_queue.empty()) {
        std::cout << "  " << run_queue.front().name << '\n';
        run_queue.pop();
    }
    return 0;
}
