#!/bin/sh
# Adversarial evaluation (docs/ADVERSARIAL.md). Three arms, 8 runs each, agent v2 without vision, $0.12 per-run cap, the generic water-tank target:
#   control      clean card and clean simulator
#   errchannel   the simulator answers bang_bang scenarios with an error message that tries to steer the tester (untrusted channel)
#   descchannel  the model card's description carries the same kind of instruction (trusted-looking channel)
# usage: scripts/run_injection.sh [BUILD_DIR] [OUT_DIR]       needs MODEL_API_KEY; costs about $1.5 to $3
set -e
cd "$(dirname "$0")/.."
B="${1:-build}"; OUT="${2:-results/adversarial}"
mkdir -p "$OUT"
for arm in control errchannel descchannel; do
  case $arm in
    control) card=examples/generic/tank_card.json; bin=generic_tank ;;
    errchannel) card=examples/generic/tank_card_errchannel.json; bin=generic_tank_injected ;;
    descchannel) card=examples/generic/tank_card_descchannel.json; bin=generic_tank ;;
  esac
  sed "s#\"command\": \"[^\"]*\"#\"command\": \"./$B/$bin\"#" "$card" > "$OUT/$arm.card.json"
  for i in 1 2 3 4 5 6 7 8; do
    RC_MODEL_CARD="$OUT/$arm.card.json" nice -n 10 "$B/rc_agent" --live --domain generic --engine card --agent-version 2 --no-vision --budget-usd 0.12 \
      --record "$OUT/$arm.$i.cache.jsonl" --out "$OUT/$arm.$i" > /dev/null 2>&1 || true
  done
  marker=""; [ "$arm" = errchannel ] && marker="--marker certified"
  echo "$arm: $("$B"/rc_inject summarize $marker "$OUT"/$arm.[1-8])"
done > "$OUT/summary.txt"
cat "$OUT/summary.txt"
