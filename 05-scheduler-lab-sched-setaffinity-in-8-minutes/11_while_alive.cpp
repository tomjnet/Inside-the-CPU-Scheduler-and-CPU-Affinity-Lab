// CPU Affinity in Code: sched_setaffinity in 8 Minutes - slide 11: thank you
// Build: make 11_while_alive
#include <iostream>
#include <queue>
#include <string>

// the imaginary tomjnet.h of the slide
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_scheduler_topic() {
    static const char* topics[] = {"Priorities, Real Time Classes and Preemption",
                                   "Core Isolation: isolcpus and nohz_full",
                                   "IRQ Affinity and Core Hygiene"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> run_queue;
    while (alive()) {
        run_queue.push(next_scheduler_topic());  // priorities are next
        run_queue.push(viewer_request());        // never starved
        subscribe();                             // pinned to TomJNet
    }

    std::cout << "run queue of the Scheduler and CPU Affinity Lab:\n";
    while (!run_queue.empty()) {
        std::cout << "  " << run_queue.front().name << "\n";
        run_queue.pop();
    }
    return 0;
}
