#!/usr/bin/env bash
# Verify the pinning from outside: look at the psr column (one CPU per worker, never changing) and at the
# one CPU masks of the workers next to the full mask of the main thread. Build first: make 08_pinned_workers
set -eu

# a long pinned pass in the background, so there is something to inspect
./08_pinned_workers 3000 pinned &
sleep 1

# one line per thread: psr is the CPU it last ran on
ps -L -o pid,tid,psr,comm -p "$(pgrep -n pinned_work)"
#   typical:   PID    TID PSR COMMAND
#             4242   4243   0 08_pinned_worke
#             4242   4244   2 08_pinned_worke

# the mask of every thread (-a), not only of the main one
taskset -a -cp "$(pgrep -n pinned_work)"
grep Cpus_allowed_list /proc/"$(pgrep -n pinned_work)"/task/*/status

wait

# did it help? migrations of the pinned run fall to about zero
perf stat -e cpu-migrations,context-switches ./08_pinned_workers
