#!/usr/bin/env bash
# Core Isolation in 9 Minutes - slide 5: the boot line with isolcpus, nohz_full and rcu_nocbs.
# Look at: the three parameters share one CPU list. Without --apply this script only prints the plan.
set -eu

if [ "${1:-}" != "--apply" ]; then
    echo "Shown, not run. Read the steps below; pass --apply only on a lab machine you can recover."
    sed -n '/^# Shown, not run/,$p' "$0"
    exit 0
fi

# Shown, not run: a wrong boot line means a trip to the recovery menu.
# 1. Edit the kernel command line: CPUs 2 and 3 become application CPUs
sudo nano /etc/default/grub
#   ISOL="isolcpus=2-3 nohz_full=2-3 rcu_nocbs=2-3"
#   GRUB_CMDLINE_LINUX_DEFAULT="quiet splash $ISOL"
# 2. Regenerate the GRUB configuration, then reboot
sudo update-grub
sudo reboot
# 3. Runtime alternative, no reboot: a cpuset partition (cgroup v2)
#   echo 2-3      > /sys/fs/cgroup/lab/cpuset.cpus
#   echo isolated > /sys/fs/cgroup/lab/cpuset.cpus.partition
