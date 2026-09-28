#!/usr/bin/env bash
# Pinning with taskset in 6 Minutes: One Process, One Core - slide 4: launch pinned, move a running process
# Look at: "CPUs visited" of each 07_spread run (one CPU when pinned), and the affinity list before and after the move.
# Needs: make 07_spread first, and a machine with at least 4 logical CPUs for the list 0,2-3.
set -eu

# Launch pinned: one CPU, a list, the same list as a hex mask
taskset -c 2 ./07_spread
taskset -c 0,2-3 ./07_spread
taskset 0xd ./07_spread
# Read, then move, a running process by its pid
sleep 60 &
taskset -cp $!
#   pid 4242's current affinity list: 0-7      (typical)
taskset -cp 2 $!
#   pid 4242's new affinity list: 2
# -a moves every thread of the process, not only the main one
taskset -acp 2 $!

# not on the slide: stop the sleeping process we used as the target
kill $!
