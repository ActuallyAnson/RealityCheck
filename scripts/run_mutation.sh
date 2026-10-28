#!/bin/sh
# Mutation benchmark driver: scripts/run_mutation.sh DOMAIN SRC_FILE RANGES COUNT SEED INVARIANTS_JSON [SCENARIOS] [WORKDIR]
# MUT_JOBS = parallel mutant builds (default 2: this is heavy on a laptop); MUT_ORDER = simultaneous mutations per mutant (default 1 = the locked benchmark)
# e.g. scripts/run_mutation.sh orbit src/sim/orbit.cpp 74-147,164-189 60 1 results/spec_orbit/spec.json
set -e
cd "$(dirname "$0")/.."
DOM="$1"; FILE="$2"; RANGES="$3"; COUNT="$4"; SEED="$5"; INV="$6"; N="${7:-1000}"; W="${8:-/tmp/mut/$1}"
rm -rf "$W"; mkdir -p "$W"
./build/rc_mutgen --file "$FILE" --ranges "$RANGES" --count "$COUNT" --seed "$SEED" --order "${MUT_ORDER:-1}" --out "$W"
./build/mut_eval --domain "$DOM" --scenarios "$N" --base-seed 700000 --invariants "$INV" --mr-scenarios 300 > "$W/clean.tsv"
ls "$W"/m*.cpp | sed 's#.*/##; s#\.cpp$##' | xargs -P "${MUT_JOBS:-2}" -I{} sh scripts/mutation_worker.sh "$W" {} "$DOM" "$INV" "$N" 700000
echo "done: $W"
