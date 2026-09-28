#!/usr/bin/env bash
# IRQ Affinity and Core Hygiene in 8 Minutes - slide 3: /proc/interrupts: who interrupts whom
# Look at: which CPU columns grow in the numbered (device) rows; watch highlights the deltas.
# Replace eth0 and nvme with the names of your own rows. Ctrl+C leaves watch.
set -eu

# One row per IRQ, one column per CPU, counts since boot (typical)
cat /proc/interrupts
#          CPU0    CPU1    CPU2    CPU3
#    24:      0       0  918204       0  PCI-MSIX  eth0-rx-0
#    25:      0       0       0  877512  PCI-MSIX  eth0-rx-1
#    31:  40211       0       0       0  PCI-MSIX  nvme0q1
#   LOC: 902113  899120  911822  915377  Local timer interrupts
#   RES:  37226   39409   51080   35507  Rescheduling interrupts

# Watch the deltas: the columns that move are the cores that pay
watch -n 1 -d "grep -E 'CPU|eth0|nvme' /proc/interrupts"
