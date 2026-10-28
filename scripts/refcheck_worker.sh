#!/bin/sh
# Scores one build under test (a mutant .cpp, as in scripts/mutation_worker.sh) with the model-written reference oracle (docs/REFERENCE_ORACLE.md).
# usage: scripts/refcheck_worker.sh WORKDIR ID DOMAIN REFRUN_BINARY SCENARIOS BASE_SEED
# Writes WORKDIR/ID.ref.tsv: scenario, steps checked, steps flagged, first flagged step, worst error/tolerance (refjudge). The reference runs sandboxed (no network, no file writes, no forking).
set -u
W="$1"; ID="$2"; DOM="$3"; REF="$4"; N="$5"; BASE="$6"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FLAGS="-O3 -DNDEBUG -std=c++17 -fno-fast-math -ffp-contract=off -w"
if ! c++ $FLAGS -I"$ROOT/src" -c "$W/$ID.cpp" -o "$W/$ID.o" 2> "$W/$ID.ref.cc.log"; then echo compile_failed > "$W/$ID.ref.status"; exit 0; fi
if ! c++ "$ROOT/build/CMakeFiles/rc_stepdump.dir/src/eval/stepdump_main.cpp.o" "$W/$ID.o" "$ROOT/build/librc_core.a" -o "$W/$ID.stepdump" 2>> "$W/$ID.ref.cc.log"; then echo compile_failed > "$W/$ID.ref.status"; exit 0; fi
echo compiled > "$W/$ID.ref.status"
(ulimit -t 300; nice -n 10 timeout 300 "$W/$ID.stepdump" --domain "$DOM" --scenarios "$N" --base-seed "$BASE" > "$W/$ID.dump" 2> "$W/$ID.ref.err")
(ulimit -t 300; nice -n 10 timeout 300 sandbox-exec -f "$ROOT/scripts/repair_sandbox.sb" "$REF" < "$W/$ID.dump" > "$W/$ID.pred" 2>> "$W/$ID.ref.err")
"$ROOT/build/rc_refjudge" --domain "$DOM" --rel "${REF_REL:-0.02}" "$W/$ID.dump" "$W/$ID.pred" > "$W/$ID.ref.tsv" 2>> "$W/$ID.ref.err"
rm -f "$W/$ID.o" "$W/$ID.stepdump"   # the dump and the predictions are kept so scripts/refcheck_rejudge.sh can apply another tolerance
