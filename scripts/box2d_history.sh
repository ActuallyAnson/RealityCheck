#!/bin/sh
# For each Box2D commit in COMMITS (third-party, MIT, cloned and built outside this repository), builds the commit and its parent, then on the same random rigid2d
# scenarios reports (1) how many scenarios violate each declared rule before vs after the commit and (2) how many scenarios differ between the two builds (rc_diff).
# A rule oracle "notices" a fix only if the counts differ; the differential oracle only says that something changed.
# usage: scripts/box2d_history.sh /path/to/box2d_clone WORK_DIR   (env: COMMITS, SCENARIOS)
set -e
cd "$(dirname "$0")/.."
CLONE="${1:?box2d clone}"; W="${2:?work dir}"; mkdir -p "$W"
N="${SCENARIOS:-1000}"
for c in $COMMITS; do
  for side in pre post; do
    ref="$c"; [ "$side" = pre ] && ref="$c^"
    [ -x "$W/${c}_$side/box2d_world" ] || sh scripts/build_box2d_hist.sh "$CLONE" "$ref" "$W/${c}_$side" > "$W/build_${c}_$side.log" 2>&1 || echo "BUILD FAILED $c $side"
  done
  for cont in 1 0; do
    for side in pre post; do
      echo "$c $side continuous=$cont: $(nice ./build/rc_hunt --engine "exec:$W/${c}_$side/box2d_world 4 $cont" --domain rigid2d --scenarios "$N" 2>&1 | grep -E 'scenarios,|scenarios ' | sed -E 's/, first at.*//' | tr '\n' ';')"
    done
    echo "$c continuous=$cont diff: $(nice ./build/rc_diff --a "exec:$W/${c}_pre/box2d_world 4 $cont" --b "exec:$W/${c}_post/box2d_world 4 $cont" --domain rigid2d --scenarios "$N" --tol 1e-4 --horizon 2 2>&1 | grep 'scenarios where')"
  done
done
