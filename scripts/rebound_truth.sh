#!/bin/sh
# Truth oracle for a REBOUND bug fix (docs/REAL_PROJECTS.md, declared in docs/PREREG_FINAL.md): runs the pre-fix and post-fix builds, REBOUND IAS15 (gate) and a
# Boost.odeint Bulirsch-Stoer truth on the same scenarios, then scores RMS position error at one frame. Scenarios any arm failed to run are dropped for all.
# usage: scripts/rebound_truth.sh PRE_BIN POST_BIN REQ.jsonl WORK_DIR [softening=0] [frame=100]   (needs build/odeint_orbit)
set -e
cd "$(dirname "$0")/.."
PRE="${1:?pre-fix rebound4_orbit}"; POST="${2:?post-fix rebound4_orbit}"; REQ="${3:?scenario file}"; W="${4:?work dir}"; export RC_SOFTENING="${5:-0}"; F="${6:-100}"
mkdir -p "$W"
[ -x "$W/frame_rms" ] || nice c++ -std=c++17 -O2 examples/external/frame_rms.cpp -Isrc -o "$W/frame_rms"
# each scenario runs in its own process with a 120 s limit (declared in docs/PREREG_FINAL.md); one that does not finish is written as a failed run
run_each() { out="$1"; shift; : > "$out"; while IFS= read -r line; do r=$(printf '%s\n' "$line" | nice timeout 120 "$@" 2>/dev/null | head -n 1); [ -n "$r" ] && echo "$r" >> "$out" || echo '{"ok":false}' >> "$out"; done < "$REQ"; }
run_each "$W/pre.out" "$PRE" trace
run_each "$W/post.out" "$POST" trace
run_each "$W/ias.out" "$POST" ias15
run_each "$W/truth.out" ./build/odeint_orbit bs 1e-12
printf "softening %s, frame %s\ngate (IAS15 vs truth): " "$RC_SOFTENING" "$F"; "$W/frame_rms" "$W/truth.out" "$W/ias.out" "$F" 2>&1 > /dev/null
"$W/frame_rms" "$W/truth.out" "$W/pre.out" "$F" > "$W/e_pre.txt" 2> /dev/null
"$W/frame_rms" "$W/truth.out" "$W/post.out" "$F" > "$W/e_post.txt" 2> /dev/null
"$W/frame_rms" "$W/pre.out" "$W/post.out" "$F" > "$W/e_pp.txt" 2> /dev/null
paste "$W/e_pre.txt" "$W/e_post.txt" "$W/e_pp.txt" | awk '
  $1 == "nan" || $2 == "nan" || $3 == "nan" { dropped++; next }
  { ok++ }
  $3 > 0 { n++; if ($2 < $1) w++; else if ($2 > $1) l++ }
  END {
    m = (w < l ? w : l); t = w + l; c = 1; s = 0
    for (k = 0; k <= m; k++) { s += c; c = c * (t - k) / (k + 1) }
    p = (t ? 2 * s / 2 ^ t : 1); if (p > 1) p = 1
    printf "scenarios run by all arms %d, dropped %d; scored (pre and post differ) %d: post closer %d, pre closer %d, ties %d; two-sided sign test p = %.2g\n", ok, dropped, n, w, l, n - w - l, p
  }'
