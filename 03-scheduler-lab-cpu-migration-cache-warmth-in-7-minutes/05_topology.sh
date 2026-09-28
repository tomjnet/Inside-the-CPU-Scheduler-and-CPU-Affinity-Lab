#!/usr/bin/env bash
# Look at: two rows of lscpu with the same CORE number are SMT siblings (nearly free to move between),
# and the L3 list names every CPU a task can move to without losing its L3 contents.
set -eu

# which logical CPU sits on which core, socket and NUMA node
lscpu -e=CPU,CORE,SOCKET,NODE,ONLINE
#   CPU CORE SOCKET NODE ONLINE      (typical: SMT, one socket)
#     0    0      0    0 yes
#     1    0      0    0 yes
#     2    1      0    0 yes
# the SMT sibling of CPU 0: same core, same L1 and L2
cat /sys/devices/system/cpu/cpu0/topology/thread_siblings_list
#   0-1
# every CPU that shares the L3 with CPU 0
cat /sys/devices/system/cpu/cpu0/cache/index3/shared_cpu_list
#   0-7
