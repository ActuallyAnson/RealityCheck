#!/bin/sh
# Evaluates a locked blind set with the oracles that need no knowledge of the bugs: hand-written rules, model-written invariants (if a spec exists),
# mined invariants, and trace difference from the clean simulator (the planter's variants are all expected to differ). Prints one line per variant.
# usage: scripts/blind_eval.sh DIR DOMAIN [SCENARIOS]
set -e
cd "$(dirname "$0")/.."
DIR="$1"; DOM="$2"; N="${3:-500}"
MAN="${MANIFEST:-data/blind/manifest_$DOM.json}"
[ -f "$MAN" ] || { echo "no manifest: run scripts/blind_lock.sh and commit it first"; exit 2; }
set -- $(grep "^$DOM " results/mutation/mine_factors.tsv)
MINE="--mine 300 --mine-seed 900000 --mine-bound $2 --mine-drift $3 --mine-step $4"
INV=results/spec_$DOM/spec.json; [ -f "$INV" ] || INV=""
W="${BLIND_WORK:-/tmp/blind/$DOM}"; mkdir -p "$W"
for f in "$DIR"/*.cpp; do
  b=$(basename "$f"); h=$(shasum -a 256 "$f" | cut -d' ' -f1)
  grep -q "\"name\":\"$b\",\"sha256\":\"$h\"" "$MAN" || { echo "REFUSED $b: not in the locked manifest (or changed since locking)"; exit 3; }
  cp "$f" "$W/$b"
done
./build/mut_eval --domain "$DOM" --scenarios "$N" --base-seed 700000 ${INV:+--invariants $INV} $MINE > "$W/clean.tsv"
for f in "$W"/*.cpp; do
  id=$(basename "$f" .cpp)
  MR_N=0 MINE_ARGS="$MINE" sh scripts/mutation_worker.sh "$W" "$id" "$DOM" "${INV:-/dev/null}" "$N" 700000
  awk -F'\t' -v id="$id" 'NR==FNR{ch[$1]=$3; cr[$1]=$4; cm[$1]=$7; next} {d+=($3!=ch[$1]); h+=($4!="-"&&cr[$1]=="-"); m+=($7!="-"&&cm[$1]=="-"); n++} END{printf "%s: differs from clean on %d/%d scenarios; hand-written rule fires on %d; mined invariant fires on %d\n", id, d, n, h, m}' "$W/clean.tsv" "$W/$id.tsv"
done
