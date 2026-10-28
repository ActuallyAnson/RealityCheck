#!/bin/sh
# Runs the locked ablation (data/eval_lock_ablation.json): random baseline, then each agent arm, then the comparison.
# Needs the API key in MODEL_API_KEY. Cost is about $2 per arm. Re-running resumes: finished runs are skipped.
set -e
cd "$(dirname "$0")/.."
LOCK=data/eval_lock_ablation.json
mkdir -p results/ablation
./build/rc_eval baseline --lock $LOCK --out results/ablation
./build/rc_eval agent --live --lock $LOCK --out results/ablation/v1 --agent-version 1 --total-budget-usd 3.5 --run-budget-usd 0.25
./build/rc_eval agent --live --lock $LOCK --out results/ablation/v2 --agent-version 2 --vision 1 --total-budget-usd 3.5 --run-budget-usd 0.25
./build/rc_eval agent --live --lock $LOCK --out results/ablation/v2-novision --agent-version 2 --vision 0 --total-budget-usd 3.5 --run-budget-usd 0.25
./build/rc_eval compare --lock $LOCK --arms v1=results/ablation/v1,v2=results/ablation/v2,v2-novision=results/ablation/v2-novision \
  --baseline results/ablation/baseline.json --out results/ablation/compare.md
