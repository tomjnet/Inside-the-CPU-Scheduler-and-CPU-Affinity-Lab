// A Repeatable Low Latency Core Recipe in 10 Minutes - slide 13: thank you
// Build: make 13_while_alive
#include <iostream>
#include <queue>
#include <string>

// the imaginary tomjnet.h of the slide
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_lab_topic() {
    static const char* topics[] = {"Interrupts and Timers Lab: the IRQ path", "Interrupts and Timers Lab: APIC routing",
                                   "Interrupts and Timers Lab: the kernel tick"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> todo;
    while (alive()) {
        todo.push(next_lab_topic());   // Interrupts and Timers Lab
        todo.push(viewer_request());   // FIFO: asked first, made first
        subscribe();                   // lifetime benefit
    }

    std::cout << "next on the channel, in FIFO order:\n";
    while (!todo.empty()) {
        std::cout << "  " << todo.front().name << '\n';
        todo.pop();
    }
    return 0;
}
