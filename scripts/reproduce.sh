#!/bin/sh
# Regenerates every table in results/ and docs/RESULTS.md from the saved raw runs, with NO API calls and no re-simulation of agent runs, and checks the
# regenerated files are byte-identical to the committed ones. Usage: scripts/reproduce.sh [BUILD_DIR]   (default: build)
set -e
cd "$(dirname "$0")/.."
B="${1:-build}"
# results/agent_hosted (agent v1 on rigid-body simulator v1) is deliberately not regenerated: its "valid and attributable" column replays each scenario on the CLEAN simulator at summarize time, and that
# simulator was fixed after those runs (v2). Regenerating now changes two cells (b04 4/5 -> 5/5, b09 0/5 -> 1/5), so the committed table is kept as the historical record (see docs/RESULTS.md).
$B/rc_eval summarize --lock data/eval_lock_orbit.json --out results/orbit_agent > /dev/null
$B/rc_eval summarize --lock data/eval_lock_cartpole.json --out results/cartpole_agent > /dev/null
$B/rc_eval summarize --lock data/eval_lock_traffic.json --out results/traffic_agent > /dev/null
$B/rc_eval compare --lock data/eval_lock_ablation.json --arms v1=results/ablation/v1,v2=results/ablation/v2,v2-novision=results/ablation/v2-novision \
  --baseline results/ablation/baseline.json --out results/ablation/compare.md > /dev/null
$B/rc_triage summarize --lock data/eval_lock_triage.json --out results/triage > /dev/null
$B/rc_spec summarize results/spec_rigid2d results/spec_orbit results/spec_cartpole results/spec_traffic > results/spec_summary.md
$B/rc_mutscore total results/mutation/orbit results/mutation/cartpole results/mutation/traffic results/mutation/rigid2d > results/mutation/total.md
sh scripts/gen_results.sh > /dev/null
if git diff --quiet -- results docs/RESULTS.md; then
  echo "reproduced: every regenerated table is identical to the committed one"
else
  echo "DIFFERENCES between regenerated and committed tables:"; git diff --stat -- results docs/RESULTS.md; exit 1
fi
