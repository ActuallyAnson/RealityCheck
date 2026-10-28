#!/bin/sh
# Calibrates the invariant miner's three tolerance factors per domain on the UNMODIFIED simulator, on scenario seeds that the
# mutation benchmark never uses (800000+; mining uses 900000+, evaluation 700000+). A fixed ladder, smallest first; the first rung with
# zero false positives on 1000 calibration scenarios is used. Output: results/mutation/mine_factors.tsv  (domain bound drift step)
# usage: scripts/mine_calibrate.sh [BIN_DIR]   (default build)
set -e
cd "$(dirname "$0")/.."
B="${1:-build}"
OUT=results/mutation/mine_factors.tsv
: > "$OUT"
for d in orbit cartpole traffic rigid2d; do
  for rung in "0.5 5 3" "1 10 6" "2 20 12" "4 40 24" "8 80 48"; do
    set -- $rung
    fp=$(nice "$B/mut_eval" --domain "$d" --scenarios 1000 --base-seed 800000 --mine 300 --mine-seed 900000 --mine-bound "$1" --mine-drift "$2" --mine-step "$3" | awk -F'\t' '$7!="-"{n++} END{print n+0}')
    echo "$d rung $rung: $fp false-positive scenarios of 1000" >&2
    if [ "$fp" = "0" ]; then echo "$d $1 $2 $3" >> "$OUT"; break; fi
  done
done
cat "$OUT"
