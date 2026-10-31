#!/bin/sh
# Smoke test for rc_scan (docs/SCAN.md): on the water-tank example (one deliberate flaw) it must report at least one finding and exit 1; with a program that
# prints nothing it must fail with a message, not crash. No model, no network. usage: scripts/scan_selftest.sh [BUILD_DIR]
cd "$(dirname "$0")/.."
B=${1:-build}
O=$(mktemp -d)
"$B/rc_scan" --cmd "$B/generic_tank" --param inflow:0:8 --param demand:0:4 --param capacity:5:60:int --param setpoint_fraction:0.1:0.9 --param gain:0.1:8:log --choice mode:proportional,bang_bang --scenarios 400 --out "$O" > "$O/log.txt" 2>&1
rc=$?
[ $rc -eq 1 ] || { echo "FAIL: expected exit 1 (findings), got $rc"; cat "$O/log.txt"; exit 1; }
grep -q "| finding |" "$O/report.md" || { echo "FAIL: no findings table"; exit 1; }
grep -q "It depends on" "$O/report.md" && grep -q "<svg" "$O/report.html" || { echo "FAIL: no reason line or no HTML plot"; exit 1; }
"$B/rc_scan" --cmd "true" --param x:0:1 --out "$O/none" > "$O/none.txt" 2>&1
[ $? -eq 2 ] && grep -q "printed nothing" "$O/none.txt" || { echo "FAIL: the empty program was not rejected clearly"; cat "$O/none.txt"; exit 1; }
echo "scan selftest ok"
rm -rf "$O"
