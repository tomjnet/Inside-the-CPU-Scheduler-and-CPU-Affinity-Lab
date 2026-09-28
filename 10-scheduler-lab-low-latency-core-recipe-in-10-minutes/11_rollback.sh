#!/usr/bin/env bash
# The undo of every stage. Look at: each line reverts one stage, and sched_rt_runtime_us must still read 950000.
# Build 09_report first (make 09_report). Needs root. Usage: PID=1234 IRQ=42 ./11_rollback.sh
set -eu
PID="${PID:-$$}"
IRQ="${IRQ:-0}"

# Rollback: one undo per stage, written down before the change
taskset -cp 0-7 "$PID"                   # stage 3: every CPU again
sudo chrt -o -p 0 "$PID"                 # stage 5: back to SCHED_OTHER
echo 0-7 | sudo tee /proc/irq/$IRQ/smp_affinity_list    # stage 7
sudo systemctl restart irqbalance
# stage 6 needs a reboot: keep the old GRUB line as a comment
# Production safety: the real time throttle stays on
cat /proc/sys/kernel/sched_rt_runtime_us
#   950000   typical: 95 percent of each second, the rest stays normal
# and every experiment is bounded
timeout 5 sudo chrt -f 50 taskset -c 2 ./09_report
