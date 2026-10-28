#!/bin/sh
# Builds and runs one mutant: scripts/mutation_worker.sh WORKDIR MUTANT_ID DOMAIN INVARIANTS_JSON SCENARIOS BASE_SEED
# Writes WORKDIR/ID.status (compiled | compile_failed), WORKDIR/ID.tsv (the evaluation) and WORKDIR/ID.exit (exit code, 124 = timeout).
set -u
W="$1"; ID="$2"; DOM="$3"; INV="$4"; N="$5"; BASE="$6"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FLAGS="-O3 -DNDEBUG -std=c++17 -fno-fast-math -ffp-contract=off -w"       # the project's flags (CMakeLists.txt), warnings off
if ! c++ $FLAGS -I"$ROOT/src" -c "$W/$ID.cpp" -o "$W/$ID.o" 2> "$W/$ID.cc.log"; then echo compile_failed > "$W/$ID.status"; exit 0; fi
if ! c++ "$ROOT/build/CMakeFiles/mut_eval.dir/src/eval/mut_eval_main.cpp.o" "$W/$ID.o" "$ROOT/build/librc_core.a" -o "$W/$ID.bin" 2>> "$W/$ID.cc.log"; then echo compile_failed > "$W/$ID.status"; exit 0; fi
echo compiled > "$W/$ID.status"
RUN=""; [ -n "${RC_SANDBOX:-}" ] && RUN="sandbox-exec -f $ROOT/scripts/repair_sandbox.sb"   # model-written builds run sandboxed (docs/REPAIR.md)
(ulimit -t 600; nice -n 10 timeout 600 $RUN "$W/$ID.bin" --domain "$DOM" --scenarios "$N" --base-seed "$BASE" --invariants "$INV" --mr-scenarios "${MR_N:-300}" > "$W/$ID.tsv" 2> "$W/$ID.err")
echo $? > "$W/$ID.exit"
rm -f "$W/$ID.o" "$W/$ID.bin"
