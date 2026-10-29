#!/bin/sh
# Builds examples/external/rebound4_orbit.cpp against ONE commit of a REBOUND clone (third-party, GPLv3; not part of this repository), in its own
# worktree and output directory, so the commit before and after a historical bug fix can be compared.
# usage: scripts/build_rebound_hist.sh /path/to/rebound_clone COMMIT OUT_DIR
set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
CLONE="${1:?path to a REBOUND clone}"; COMMIT="${2:?commit}"; OUT="${3:?output dir}"
case "$OUT" in /*) ;; *) OUT="$ROOT/$OUT";; esac
WT="$OUT/src"
mkdir -p "$OUT"
[ -d "$WT" ] || git -C "$CLONE" worktree add --detach "$WT" "$COMMIT" > /dev/null
mkdir -p "$OUT/obj"
for f in "$WT"/src/*.c; do
  case "$(basename "$f")" in glad.c|communication_mpi.c) continue;; esac
  nice cc -O2 -c "$f" -I"$WT/src" -o "$OUT/obj/$(basename "$f" .c).o" 2>/dev/null
done
FLAGS=""
grep -q REB_INTEGRATOR_TRACE "$WT/src/rebound.h" || FLAGS="$FLAGS -DRB_NO_TRACE"
grep -q REB_INTEGRATOR_BS "$WT/src/rebound.h" || FLAGS="$FLAGS -DRB_NO_BS"
nice c++ -std=c++17 -O2 $FLAGS examples/external/rebound4_orbit.cpp "$OUT"/obj/*.o -I"$WT/src" -Isrc -lm -o "$OUT/rebound4_orbit"
echo "built $OUT/rebound4_orbit at $(git -C "$WT" rev-parse --short HEAD)"
