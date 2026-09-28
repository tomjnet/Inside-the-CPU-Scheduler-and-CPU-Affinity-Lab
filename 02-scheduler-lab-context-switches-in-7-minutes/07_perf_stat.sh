#!/usr/bin/env bash
# Context Switches in 7 Minutes: The Hidden Cost - slide 7: perf stat counts for us
# Look at: the context-switches line (a few hundred for four spinners on one CPU, about 200 for the
# sleep loop) and the cs column of vmstat. Build first with make; perf needs real Linux, not WSL.
set -eu

# the kernel counts for us: no change in the program
perf stat -e context-switches,cpu-migrations ./06_oversubscribe
#   typical, 4 spinners sharing one core for a second:
#            310      context-switches
#              2      cpu-migrations

# squeeze any program onto one core from the shell and compare
taskset -c 0 perf stat -e context-switches ./05_sleep_vs_spin

# the whole machine: switches per second in the column cs
vmstat 1 5
