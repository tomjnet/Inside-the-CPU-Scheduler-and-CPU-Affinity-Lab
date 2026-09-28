#!/usr/bin/env bash
# Look at: cpu-migrations of the whole run, then se.nr_migrations and the PSR column while it runs.
# Build first with: make 07_under_load. perf needs the linux-tools package and real Linux hardware.
set -eu

# migrations of one run, counted by the kernel
perf stat -e cpu-migrations,context-switches ./07_under_load
#        14      cpu-migrations       (typical)
#     2,113      context-switches
# the per task counter, read while the program runs
./07_under_load &
sleep 1.8
grep nr_migrations /proc/$!/sched
#   se.nr_migrations          :          9
# the CPU each thread last ran on: column PSR
ps -L -o tid,psr,comm -p $!
wait
