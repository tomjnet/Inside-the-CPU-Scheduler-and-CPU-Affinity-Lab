#!/usr/bin/env bash
# Measure Scheduler Jitter in 9 Minutes: Wakeup Latency and Tails - slide 10: cyclictest and perf sched
# Look at: the Max column of cyclictest (microseconds) and the histogram rows far from the peak,
# then the switch and migration counts of CPU 2 and the maximum delay column of perf sched latency.
# Needs root and real Linux hardware. Replace CPU 2 with your application core. WSL has no perf counters.
set -eu

# the standard tool, from rt-tests: needs root for SCHED_FIFO
sudo apt install rt-tests
# 1 thread on CPU 2, priority 80, every 1000 us, 5000 loops
sudo cyclictest -m -t 1 -a 2 -p 80 -i 1000 -l 5000 -q
#   typical: T: 0 P:80 I:1000 C:5000 Min: 3 Act: 5 Avg: 6 Max: 41
# the same run as a histogram: 400 buckets of 1 microsecond
sudo cyclictest -m -t 1 -a 2 -p 80 -i 1000 -l 5000 -q -h 400
# switches and migrations on that CPU: episodes 2 and 3
sudo perf stat -e context-switches,cpu-migrations -C 2 sleep 5
# the scheduler's own record of wakeup delays, per task
sudo perf sched record -- sleep 5
sudo perf sched latency --sort max
