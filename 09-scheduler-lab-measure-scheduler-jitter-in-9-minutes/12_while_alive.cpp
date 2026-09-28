// Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 12: thank you
// Build: make 12_while_alive
#include <iostream>
#include <string>
#include <vector>

// the imaginary tomjnet.h of the slide
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_scheduler_topic() {
    static const char* topics[] = {"A Repeatable Low Latency Core Recipe in 10 Minutes",
                                   "Interrupts and Timers Lab: the IRQ path",
                                   "Low Latency C++ Lab: the whole loop, measured"};
    return Topic{topics[episodes++]};
}
struct Histogram {
    std::vector<Topic> seen;
    void record(const Topic& t) {
        seen.push_back(t);
        std::cout << "episode " << seen.size() << ": " << t.name << "\n";
    }
};
static void measure_again() { std::cout << "  measured again: p50, p99, p99.9, max\n"; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    Histogram channel;                   // p50, p99, p99.9, max
    while (alive()) {
        channel.record(next_scheduler_topic());   // the recipe is next
        measure_again();                 // one change at a time
        subscribe();                     // zero jitter, guaranteed
    }
    std::cout << channel.seen.size() << " topics recorded\n";
    return 0;
}
