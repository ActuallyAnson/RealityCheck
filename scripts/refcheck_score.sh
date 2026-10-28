#!/bin/sh
# Scores the stored reference-oracle outputs (results/reforacle/*) against the locked bug sets. No model; needs build_dev or build with rc_refscore.
cd "$(dirname "$0")/.."
B=${BUILD_DIR:-build}
for d in orbit cartpole; do
  echo "## $d: 40 sampled mutants (results/mutation/$d/mutation.json)"
  "$B/rc_refscore" mutants results/mutation/$d/mutation.json results/reforacle/$d/mutants | sed -n '/^$/,$p'
  echo "## $d: model-planted variants (results/blind/eval_$d.txt)"
  "$B/rc_refscore" planted results/blind/eval_$d.txt results/reforacle/$d/planted | sed -n '/^$/,$p'
done
