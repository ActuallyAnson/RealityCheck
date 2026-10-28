#!/bin/sh
# Checks the blind-set guard without a planter: a lock followed by an edited file must be refused. Uses a throwaway copy; leaves no manifest behind.
set -e
cd "$(dirname "$0")/.."
T=$(mktemp -d); cp /tmp/mut/orbit/m001.cpp "$T/v1.cpp" 2>/dev/null || cp src/sim/orbit.cpp "$T/v1.cpp"
sh scripts/blind_lock.sh "$T" selftest >/dev/null
echo "// tampered" >> "$T/v1.cpp"
if ./scripts/blind_eval.sh "$T" selftest 10 2>&1 | grep -q REFUSED; then echo "selftest ok: tampered file refused"; else echo "selftest FAILED"; rm -f data/blind/manifest_selftest.json; exit 1; fi
rm -f data/blind/manifest_selftest.json
