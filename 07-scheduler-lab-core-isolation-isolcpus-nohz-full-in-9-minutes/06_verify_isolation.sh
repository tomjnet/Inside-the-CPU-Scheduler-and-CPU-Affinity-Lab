#!/usr/bin/env bash
# Core Isolation in 9 Minutes - slide 6: verify the boot parameters from /proc and /sys.
# Look at: the same CPU list in cmdline, isolated and nohz_full; an empty line means nothing is isolated.
set -eu

# 1. The boot line the kernel really received
cat /proc/cmdline
# 2. The kernel's own view: CPU lists, an empty line means none
cat /sys/devices/system/cpu/isolated
cat /sys/devices/system/cpu/nohz_full || echo "(no nohz_full file: kernel built without NO_HZ_FULL)"
#   2-3          typical on the isolated lab machine
# 3. Was the kernel built for it, and how fast is the tick
grep -E 'NO_HZ_FULL|CONFIG_HZ=' /boot/config-$(uname -r) || echo "(no kernel config under /boot: WSL?)"
# 4. Who still lives on CPU 2: only per CPU kernel threads should
ps -eo psr,pid,cls,comm | awk '$1 == 2'
# 5. Run the worker there: nobody else will
[ -x ./09_pin_isolated ] || make CXX=g++ 09_pin_isolated
taskset -c 2 ./09_pin_isolated
