#!/bin/sh
# Summarizes results/blind/eval_*.txt (written by scripts/blind_eval.sh) into results/blind/summary.md. No model.
cd "$(dirname "$0")/.."
{
  echo "| domain | variants | changed behavior | a hand-written rule fires | a mined invariant fires | changed, no hand-written rule fires | of those, mined invariant fires |"
  echo "|---|---|---|---|---|---|---|"
  for d in orbit cartpole traffic rigid2d; do
    awk -v d="$d" '{ split($0, a, ": differs from clean on "); split(a[2], b, "/"); df = b[1] + 0;
        match($0, /rule fires on [0-9]+/); h = substr($0, RSTART + 14, RLENGTH - 14) + 0;
        match($0, /invariant fires on [0-9]+/); m = substr($0, RSTART + 18, RLENGTH - 18) + 0;
        n++; if (df > 0) ch++; if (h > 0) hr++; if (m > 0) mn++; if (df > 0 && h == 0) { sil++; if (m > 0) ms++ }
        N += 1 }
      END { printf "| %s | %d | %d | %d | %d | %d | %d |\n", d, n, ch, hr, mn, sil, ms }' results/blind/eval_$d.txt
  done
} > results/blind/summary.md
awk -F'|' 'NR>2 { n+=$3; c+=$4; h+=$5; m+=$6; s+=$7; ms+=$8 } END { printf "| total | %d | %d | %d | %d | %d | %d |\n", n, c, h, m, s, ms }' results/blind/summary.md >> results/blind/summary.md
cat results/blind/summary.md
