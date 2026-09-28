// CPU Migration and Cache Warmth in 7 Minutes - slide 9: thank you
// Build: make 09_while_alive
#include <iostream>
#include <queue>
#include <string>

// the imaginary tomjnet.h of the slide, written out so the program builds
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_scheduler_topic() {
    static const char* topics[] = {"Pinning with taskset: one process, one core",
                                   "CPU affinity in code: sched_setaffinity",
                                   "Priorities, real time classes and preemption"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> run_queue;
    while (alive()) {
        run_queue.push(next_scheduler_topic());  // next: taskset
        run_queue.push(viewer_request());        // wakes up first
        subscribe();                             // never migrates
    }

    std::cout << "run queue of the Scheduler and CPU Affinity Lab, " << run_queue.size() << " entries:\n";
    while (!run_queue.empty()) {
        std::cout << "  " << run_queue.front().name << '\n';
        run_queue.pop();
    }
    return 0;
}
