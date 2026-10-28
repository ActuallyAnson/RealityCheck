#!/bin/sh
# Scores the invariant miner on the locked mutation benchmark (docs/MINING.md). Needs the mutants from scripts/run_mutation_all.sh
# (in /tmp/mut/DOMAIN) and results/mutation/mine_factors.tsv from scripts/mine_calibrate.sh. MUT_JOBS parallel builds (default 2).
set -e
cd "$(dirname "$0")/.."
for d in orbit cartpole traffic rigid2d; do
  set -- $(grep "^$d " results/mutation/mine_factors.tsv)
  export MINE_ARGS="--mine 300 --mine-seed 900000 --mine-bound $2 --mine-drift $3 --mine-step $4"
  W=/tmp/mut_mine/$d
  mkdir -p "$W"
  cp /tmp/mut/$d/m0*.cpp /tmp/mut/$d/index.json "$W"/
  # shellcheck disable=SC2086
  nice ./build/mut_eval --domain "$d" --scenarios 1000 --base-seed 700000 --invariants results/spec_$d/spec.json $MINE_ARGS > "$W/clean.tsv"
  ls "$W"/m*.cpp | sed 's#.*/##; s#\.cpp$##' | MR_N=0 xargs -P "${MUT_JOBS:-2}" -I{} sh scripts/mutation_worker.sh "$W" {} "$d" results/spec_$d/spec.json 1000 700000
  ./build/rc_minescore --dir "$W" --mutation results/mutation/$d/mutation.json --domain "$d"
done
