#!/usr/bin/env bash
# Look at: the 950000 of the throttling file, the policy line chrt prints for this shell, and the CLS column
# of ps (TS = fair, FF = SCHED_FIFO, RR = SCHED_RR). Run make first; the timeout line needs root.
set -eu

# The safety net: real time tasks get 95 percent of each second
cat /proc/sys/kernel/sched_rt_runtime_us
#   950000     (typical, of a 1000000 microsecond period)
# Rule: every real time experiment runs under a timeout
sudo timeout 5 chrt -f 50 ./09_bounded_experiment
# Inspect: policy and priority of this shell, then of every task
chrt -p $$
ps -eo pid,cls,rtprio,ni,comm | head -4
#   PID CLS RTPRIO  NI COMMAND
#     1  TS      -   0 systemd
#    17  FF     99   - migration/0
