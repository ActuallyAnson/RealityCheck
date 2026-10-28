#!/bin/sh
# Second-order mutants (two simultaneous changes on distinct lines), fresh sampling seed 2, for the harder repair evaluation (docs/REPAIR.md, second-order section).
# Cart-pole and traffic only (cheap simulators). Results: results/mutation2/DOMAIN. MUT_JOBS parallel builds (default 2).
set -e
cd "$(dirname "$0")/.."
export MUT_ORDER=2
sh scripts/run_mutation.sh cartpole src/sim/cartpole.cpp 94-139,147-197 30 2 results/spec_cartpole/spec.json 1000 /tmp/mut2/cartpole
sh scripts/run_mutation.sh traffic src/sim/traffic.cpp 112-253 30 2 results/spec_traffic/spec.json 1000 /tmp/mut2/traffic
for d in cartpole traffic; do ./build/rc_mutscore --dir /tmp/mut2/$d --domain $d --out results/mutation2/$d > /dev/null; done
echo "done: results/mutation2/*"
