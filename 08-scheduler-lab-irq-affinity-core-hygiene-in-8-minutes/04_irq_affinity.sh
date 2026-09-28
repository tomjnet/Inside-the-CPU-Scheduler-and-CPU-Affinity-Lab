#!/usr/bin/env bash
# IRQ Affinity and Core Hygiene in 8 Minutes - slide 4: one mask per irq: smp_affinity_list
# Look at: the allowed list against the effective CPU, before and after the write (root only).
# Replace 24 with an IRQ number of your machine; a managed IRQ answers with an input/output error.
set -eu

# Which CPUs may handle IRQ 24? A list here, a hex mask next to it
cat /proc/irq/24/smp_affinity_list
#   0-7
# Where the hardware really delivers it right now (typical)
cat /proc/irq/24/effective_affinity_list
#   2

# Move it to the housekeeping cores 0 and 1: the write needs root
echo 0-1 | sudo tee /proc/irq/24/smp_affinity_list

# IRQs registered later start from this mask (hex, ff = CPUs 0 to 7)
cat /proc/irq/default_smp_affinity
