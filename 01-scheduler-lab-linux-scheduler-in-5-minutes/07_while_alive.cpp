// The Linux Scheduler in 5 Minutes: Who Runs Next? - slide 7: thank you
// Build: make 07_while_alive
#include <iostream>
#include <queue>
#include <string>

// the imaginary tomjnet.h of the slide
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_lab_topic() {
    static const char* topics[] = {"context switches and their hidden cost", "CPU migration and cache warmth",
                                   "pinning with taskset"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> run_queue;               // one per CPU
    while (alive()) {
        run_queue.push(next_lab_topic());      // context switches
        run_queue.push(viewer_request());      // runnable, waiting
        subscribe();                           // never preempted
    }

    std::cout << "runnable topics: " << run_queue.size() << "\n";
    while (!run_queue.empty()) {               // first in, first out: the scheduler picks the front
        std::cout << "  running: " << run_queue.front().name << "\n";
        run_queue.pop();
    }
    return 0;
}
