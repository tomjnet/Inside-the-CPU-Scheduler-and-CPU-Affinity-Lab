#!/usr/bin/env bash
# The Linux Scheduler in 5 Minutes: Who Runs Next? - slide 5: in the lab: states and cpus with ps
# Look at: the STAT column (mostly S, a few R), the PSR column (the CPU each task last ran on),
# the CLS column (TS = fair class, FF or RR = real time), then the counters of the shell itself.
set -eu

# every task: pid, last CPU (psr), state, class, name
ps -eo pid,psr,stat,cls,comm | head -4
#   PID PSR STAT CLS COMMAND        (typical)
#     1   3 Ss    TS systemd
#    17   0 S     FF migration/0
#  1234   5 R+    TS ps

# how many tasks are in each state right now (first letter of STAT)
ps -eo stat= | cut -c1 | sort | uniq -c

# one task in detail: the shell itself ($$ is its pid)
# /proc/<pid>/sched needs a kernel built with scheduler debug; status always has the switch counters
if [ -r /proc/$$/sched ]; then
    grep -E 'nr_switches|nr_migrations|policy' /proc/$$/sched
else
    grep -E 'ctxt_switches' /proc/$$/status
fi
#   se.nr_migrations      :      4  (typical)
#   nr_switches           :    212
#   policy                :      0  (0 is SCHED_OTHER, the fair class)
