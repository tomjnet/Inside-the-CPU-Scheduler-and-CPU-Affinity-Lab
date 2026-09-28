#!/usr/bin/env bash
# IRQ Affinity and Core Hygiene in 8 Minutes - slide 6: irqbalance and its banned cpus
# Look at: is the daemon active? (if not, status fails and the script stops: nothing to tame). Then pick ONE option.
# Replace 2-3 with your application cores. Needs root and a real Linux install (not WSL).
set -eu

# irqbalance rewrites the masks about every 10 seconds: is it running?
systemctl status irqbalance --no-pager

# Ban the application cores 2 and 3, then restart the daemon
echo 'IRQBALANCE_BANNED_CPULIST=2-3' |
    sudo tee -a /etc/default/irqbalance
sudo systemctl restart irqbalance

# Or stop it and own every mask yourself, from a boot script
sudo systemctl disable --now irqbalance
