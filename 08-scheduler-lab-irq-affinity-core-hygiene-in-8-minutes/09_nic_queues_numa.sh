#!/usr/bin/env bash
# IRQ Affinity and Core Hygiene in 8 Minutes - slide 9: nic queues and numa locality
# Look at: one row per NIC queue, the queue count, and the NUMA node of the card (-1: one node).
# Replace eth0 and 24 with your interface and IRQ. Needs root and real hardware for the write.
set -eu

# One MSI-X vector per NIC queue: list them, then count the queues
grep eth0 /proc/interrupts
ethtool -l eth0

# Which NUMA node is the NIC on, and which CPUs live there? (typical)
cat /sys/class/net/eth0/device/numa_node
#   0
lscpu | grep 'NUMA node0'
#   NUMA node0 CPU(s):   0-15

# Queue 0 (IRQ 24) goes to a housekeeping core of the same node
echo 1 | sudo tee /proc/irq/24/smp_affinity_list
