#!/bin/sh
# Re-applies the judge with another relative tolerance to the kept dumps and predictions of scripts/refcheck_worker.sh.
# usage: scripts/refcheck_rejudge.sh WORKDIR DOMAIN REL OUT_SUFFIX     (writes WORKDIR/ID.ref<OUT_SUFFIX>.tsv)
W="$1"; DOM="$2"; REL="$3"; SUF="$4"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
for d in "$W"/*.dump; do id=$(basename "$d" .dump); "$ROOT/build/rc_refjudge" --domain "$DOM" --rel "$REL" "$d" "$W/$id.pred" > "$W/$id.ref$SUF.tsv" 2>/dev/null; done
