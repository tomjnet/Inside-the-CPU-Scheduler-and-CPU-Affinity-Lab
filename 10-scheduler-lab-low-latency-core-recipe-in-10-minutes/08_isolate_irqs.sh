#!/usr/bin/env bash
# Stages 6 and 7 of the recipe. Look at: isolated and nohz_full must name your application CPUs, and after the
# loop the NIC counters of /proc/interrupts must grow only in the housekeeping columns. Needs root and real hardware.
set -eu
NIC="${NIC:-eth0}"

# Stage 6: is the core isolated? Boot parameters from episode 7
grep -o 'isolcpus=[^ ]*\|nohz_full=[^ ]*\|rcu_nocbs=[^ ]*' /proc/cmdline || true
#   isolcpus=2,6  nohz_full=2,6  rcu_nocbs=2,6        typical
cat /sys/devices/system/cpu/isolated /sys/devices/system/cpu/nohz_full
# Stage 7: move the NIC vectors to the housekeeping CPUs (needs root)
grep -E "CPU0|$NIC" /proc/interrupts | cut -c1-72
for irq in $(grep "$NIC" /proc/interrupts | cut -d: -f1); do
  echo 0-1 | sudo tee /proc/irq/$irq/smp_affinity_list
done
# and tell irqbalance to stay away: IRQBALANCE_BANNED_CPULIST=2,6
