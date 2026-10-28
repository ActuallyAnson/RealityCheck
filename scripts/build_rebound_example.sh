#!/bin/sh
# Builds examples/external/rebound_orbit.cpp against a REBOUND checkout (third-party, GPLv3; not part of this repository).
# usage: scripts/build_rebound_example.sh /path/to/rebound [out_dir]      (default out_dir: build)
set -e
cd "$(dirname "$0")/.."
RB="${1:?path to a REBOUND checkout}"; OUT="${2:-build}"
mkdir -p "$OUT/rebound_obj"
for f in "$RB"/src/*.c; do
  case "$(basename "$f")" in glad.c|communication_mpi.c) continue;; esac
  cc -O2 -c "$f" -I"$RB/src" -o "$OUT/rebound_obj/$(basename "$f" .c).o"
done
c++ -std=c++17 -O2 examples/external/rebound_orbit.cpp "$OUT"/rebound_obj/*.o -I"$RB/src" -Isrc -lm -o "$OUT/rebound_orbit"
echo "built $OUT/rebound_orbit"
