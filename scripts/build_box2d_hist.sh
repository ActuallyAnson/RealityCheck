#!/bin/sh
# Builds examples/external/box2d_world.cpp against ONE commit of a Box2D clone (third-party, MIT; not part of this repository), in its own worktree and
# output directory. usage: scripts/build_box2d_hist.sh /path/to/box2d_clone COMMIT OUT_DIR [ADAPTER.cpp]  (default adapter: box2d_world.cpp)
set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
CLONE="${1:?path to a Box2D clone}"; COMMIT="${2:?commit}"; OUT="${3:?output dir}"; SRC="${4:-examples/external/box2d_world.cpp}"; BIN="$(basename "$SRC" .cpp)"
case "$OUT" in /*) ;; *) OUT="$ROOT/$OUT";; esac
WT="$OUT/src"
mkdir -p "$OUT"
[ -d "$WT" ] || git -C "$CLONE" worktree add --detach "$WT" "$COMMIT" > /dev/null
nice cmake -S "$WT" -B "$OUT/cmake" -DCMAKE_BUILD_TYPE=Release -DBOX2D_SAMPLES=OFF -DBOX2D_UNIT_TESTS=OFF -DBOX2D_BENCHMARKS=OFF -DBOX2D_DOCS=OFF -DBOX2D_VALIDATE=OFF -DBUILD_SHARED_LIBS=OFF > "$OUT/cmake.log" 2>&1
nice cmake --build "$OUT/cmake" --target box2d -j 2 >> "$OUT/cmake.log" 2>&1
LIB="$(find "$OUT/cmake" -name 'libbox2d.a' | head -1)"
nice c++ -std=c++17 -O2 "$SRC" "$LIB" -I"$WT/include" -Isrc -o "$OUT/$BIN"
echo "built $OUT/$BIN at $(git -C "$WT" rev-parse --short HEAD)"
