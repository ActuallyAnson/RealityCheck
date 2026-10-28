#!/bin/sh
# The locked mutation benchmark: 40 mutants per domain, sampling seed 1, a pool of 1000 random scenarios, 300000-step windows.
# Mutated line ranges cover each simulator's step function and its force/integration/controller helpers. They exclude the
# scenario sampler, JSON and validation code, the seeded-bug switches (dead code on the clean path), and the energy helpers
# the rule checker itself uses (mutating the oracle would mutate the test, not the system under test).
set -e
cd "$(dirname "$0")/.."
sh scripts/run_mutation.sh orbit src/sim/orbit.cpp 74-146,164-189 40 1 results/spec_orbit/spec.json 1000
sh scripts/run_mutation.sh cartpole src/sim/cartpole.cpp 94-139,147-197 40 1 results/spec_cartpole/spec.json 1000
sh scripts/run_mutation.sh traffic src/sim/traffic.cpp 112-253 40 1 results/spec_traffic/spec.json 1000
# rigid2d mutants were generated from a world.cpp that has since changed (generic-domain edits shifted lines); the pinned source is data/mutation_base/rigid2d_world.cpp
# (checked: rc_mutgen on it reproduces the locked mutants byte for byte)
sh scripts/run_mutation.sh rigid2d data/mutation_base/rigid2d_world.cpp 25-256,259-283 40 1 results/spec_rigid2d/spec.json 1000
for d in orbit cartpole traffic rigid2d; do ./build/rc_mutscore --dir /tmp/mut/$d --domain $d --out results/mutation/$d > /dev/null; done
echo "all domains done: results/mutation/*/mutation.md"
