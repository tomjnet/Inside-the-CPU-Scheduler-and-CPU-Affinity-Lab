// Priorities, Real Time Classes and Preemption in 9 Minutes - slide 10: priority inversion and starvation
// Build: make 10_inversion_model
#include <iostream>
#include <string>

struct Task {
    int prio;
    int work;            // ticks of CPU still needed
    bool ready;
    bool blocked_on_L;   // only meaningful for H
};

// One core. H waits for the lock that L holds. M needs no lock.
char pick(const Task& L, const Task& M, const Task& H, bool inherit) {
    int l = L.prio;
    if (inherit && H.blocked_on_L) l = H.prio;  // boost the holder
    if (H.ready && !H.blocked_on_L) return 'H';
    if (M.ready && M.prio > l) return 'M';      // inversion: M beats H
    return 'L';                 // L finishes, unlocks, H runs at last
}

struct Outcome {
    std::string timeline;
    int h_waited;
};

static Outcome play(bool inherit) {
    Task L{10, 3, true, false};    // holds the lock for its 3 ticks of critical section
    Task M{50, 6, false, false};   // wakes at tick 1, needs no lock
    Task H{90, 2, false, false};   // wakes at tick 1, needs the lock first
    Outcome out{"", 0};
    for (int tick = 0; L.work + M.work + H.work > 0; ++tick) {
        if (tick == 1) {
            M.ready = true;
            H.ready = true;
        }
        H.blocked_on_L = H.ready && L.work > 0;
        char who = pick(L, M, H, inherit);
        Task& t = who == 'H' ? H : who == 'M' ? M : L;
        if (H.ready && who != 'H') ++out.h_waited;
        if (--t.work == 0) t.ready = false;
        out.timeline += who;
    }
    return out;
}

int main() {
    std::cout << "one core, one tick per letter; L prio 10 holds the lock, M prio 50 and H prio 90 wake at tick 1\n";
    Outcome plain = play(false);
    Outcome boosted = play(true);
    std::cout << "  without inheritance: " << plain.timeline << "   H waited " << plain.h_waited << " ticks\n";
    std::cout << "  with inheritance:    " << boosted.timeline << "   H waited " << boosted.h_waited << " ticks\n";
    std::cout << "without it H waits for all of M, a task that never touches the lock; with it H waits only\n"
                 "for the rest of the critical section. On Linux: a pthread mutex with PTHREAD_PRIO_INHERIT.\n";
    bool ok = plain.timeline == "LMMMMMMLLHH" && boosted.timeline == "LLLHHMMMMMM";
    std::cout << (ok ? "model check: OK\n" : "model check: FAILED\n");
    return ok ? 0 : 1;
}
