#!/bin/sh
# Ball-drop truth oracle for Box2D commits (docs/REAL_PROJECTS.md, declared in docs/PREREG_FINAL.md). usage: scripts/box2d_ball_truth.sh WORK_DIR PRE_BIN POST_BIN [seed=21] [n=1000]
# Builds the analytic-truth tool, generates the scenarios, runs both builds (substeps 4, continuous on) and scores RMS centre-height error against the truth.
set -e
cd "$(dirname "$0")/.."
W="${1:?work dir}"; PRE="${2:?pre-fix box2d_world}"; POST="${3:?post-fix box2d_world}"; SEED="${4:-21}"; N="${5:-1000}"
mkdir -p "$W"
nice c++ -std=c++17 -O2 examples/external/ball_truth.cpp -Isrc -o "$W/ball_truth"
"$W/ball_truth" gen "$SEED" "$N" > "$W/req.jsonl"
nice "$PRE" 4 1 < "$W/req.jsonl" > "$W/pre.out"
nice "$POST" 4 1 < "$W/req.jsonl" > "$W/post.out"
"$W/ball_truth" score "$W/req.jsonl" "$W/pre.out" "$W/post.out"
