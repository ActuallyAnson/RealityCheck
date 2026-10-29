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
for d in orbit cartpole; do  # a second, independent draw of the reference (docs/REFERENCE_ORACLE.md)
  [ -d results/reforacle/$d/draw2/mutants ] || continue
  echo "## $d draw2: 40 sampled mutants"
  "$B/rc_refscore" mutants results/mutation/$d/mutation.json results/reforacle/$d/draw2/mutants | sed -n '/^$/,$p'
  echo "## $d draw2: model-planted variants"
  "$B/rc_refscore" planted results/blind/eval_$d.txt results/reforacle/$d/draw2/planted | sed -n '/^$/,$p'
done
for draw in draw2 draw3; do  # traffic: two independent draws of the reference (draw 2 is the pre-declared headline)
  [ -d results/reforacle/traffic/$draw/mutants ] || continue
  echo "## traffic $draw: 40 sampled mutants (results/mutation/traffic/mutation.json)"
  "$B/rc_refscore" mutants results/mutation/traffic/mutation.json results/reforacle/traffic/$draw/mutants | sed -n '/^$/,$p'
  [ -d results/reforacle/traffic/$draw/planted ] || continue
  echo "## traffic $draw: model-planted variants (results/blind/eval_traffic.txt)"
  "$B/rc_refscore" planted results/blind/eval_traffic.txt results/reforacle/traffic/$draw/planted | sed -n '/^$/,$p'
done
