#!/bin/sh
# Locks a blind bug set. A planter other than the tester writes variants of a simulator (one self-contained .cpp per variant, same interface as the
# mutants in /tmp/mut/DOMAIN, see docs/BLIND.md) into DIR. This script records only the SHA-256 of each file and the time and commit; commit the manifest
# BEFORE any tester or oracle is run on the set. scripts/blind_eval.sh later refuses a file whose hash is not in the manifest.
# usage: scripts/blind_lock.sh DIR DOMAIN
set -e
cd "$(dirname "$0")/.."
DIR="$1"; DOM="$2"; OUT="${MANIFEST:-data/blind/manifest_$DOM.json}"
mkdir -p data/blind
{
  printf '{"domain":"%s","locked_utc":"%s","repo_commit":"%s","files":[' "$DOM" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$(git rev-parse --short HEAD)"
  first=1
  for f in "$DIR"/*.cpp; do
    h=$(shasum -a 256 "$f" | cut -d' ' -f1)
    [ $first = 1 ] || printf ','
    first=0
    printf '{"name":"%s","sha256":"%s"}' "$(basename "$f")" "$h"
  done
  printf ']}\n'
} > "$OUT"
echo "wrote $OUT ($(grep -o sha256 "$OUT" | wc -l | tr -d ' ') files). Commit it before running anything on the set."
