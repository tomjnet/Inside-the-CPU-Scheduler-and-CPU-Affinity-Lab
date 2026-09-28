#!/usr/bin/env bash
# Stage 2 of the recipe. Look at: which logical CPUs share a CORE value (SMT siblings) and the NUMA node of
# the NIC; write down the application CPU, its idle sibling and the housekeeping CPUs. NIC=enp3s0 ./05_topology.sh
set -eu
NIC="${NIC:-eth0}"

# Stage 2: topology. One physical core, not two SMT siblings
lscpu -e=CPU,CORE,SOCKET,NODE,ONLINE
cat /sys/devices/system/cpu/cpu2/topology/thread_siblings_list
#   2,6      typical: CPU 2 and CPU 6 share one physical core
# which NUMA node owns the NIC (-1 means a single node)
cat /sys/class/net/$NIC/device/numa_node
numactl --hardware | head -3
