#!/usr/bin/env bash
# Pinning with taskset in 6 Minutes: One Process, One Core - slide 5: physical cores, not smt siblings
# Look at: rows of lscpu that share a CORE number are SMT siblings; pin two workers to CPUs with different CORE numbers.
# WSL and virtual machines may show a simplified topology: read it on the real Linux machine you will measure on.
set -eu

# Which logical CPUs share one physical core?
lscpu -e=CPU,CORE,SOCKET,ONLINE
#   CPU CORE SOCKET ONLINE      (typical: 4 cores with SMT)
#     0    0      0    yes
#     1    0      0    yes      CPU 0 and CPU 1: the same core
#     2    1      0    yes
cat /sys/devices/system/cpu/cpu2/topology/thread_siblings_list
#   2-3
