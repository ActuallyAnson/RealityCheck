#!/bin/sh
# Re-validates every recorded triage report (three models, 83 failures each) with the default and the strict validator; no model, no network.
# Writes results/validator/strict.txt. Needs a build (default ./build).
set -e
B=${BUILD_DIR:-build}
mkdir -p results/validator
{
  for d in triage triage_spark12_full triage_spark11_full; do
    echo "## results/$d"
    "$B/rc_strictcheck" results/$d/runs | grep -v '^  e.g.'
    echo
  done
} > results/validator/strict.txt
cat results/validator/strict.txt
