#!/bin/sh
# Version-regression hunt on REBOUND (third-party, GPLv3, cloned and built outside this repository): builds the 4.x-era release tags in TAGS and runs the orbit
# domain through each consecutive pair with rc_diff (pure differential test, no rules, no model). Any scenario where two releases disagree is a lead to check
# against the changelog, not a bug claim.
# usage: scripts/rebound_history.sh /path/to/rebound_clone WORK_DIR   (env: TAGS, SCENARIOS, INTEGRATORS)
set -e
cd "$(dirname "$0")/.."
CLONE="${1:?rebound clone}"; W="${2:?work dir}"; mkdir -p "$W"
TAGS="${TAGS:-4.0.0 4.1.0 4.3.0 4.4.0 4.4.5 4.4.11 4.5.1 4.6.0}"
N="${SCENARIOS:-300}"; INTEG="${INTEGRATORS:-leapfrog ias15 whfast mercurius bs}"
for t in $TAGS; do [ -x "$W/$t/rebound4_orbit" ] || sh scripts/build_rebound_hist.sh "$CLONE" "$t" "$W/$t" > "$W/build_$t.log" 2>&1 || echo "BUILD FAILED $t"; done
prev=""
for t in $TAGS; do
  if [ -n "$prev" ] && [ -x "$W/$t/rebound4_orbit" ] && [ -x "$W/$prev/rebound4_orbit" ]; then
    for i in $INTEG; do
      r=$(nice ./build/rc_diff --a "exec:$W/$prev/rebound4_orbit $i 1" --b "exec:$W/$t/rebound4_orbit $i 1" --domain orbit --scenarios "$N" --tol 1e-9 --horizon 100 2>&1 | grep -E "scenarios where" || echo "no result")
      echo "$prev -> $t  $i  $r"
    done
  fi
  prev="$t"
done
