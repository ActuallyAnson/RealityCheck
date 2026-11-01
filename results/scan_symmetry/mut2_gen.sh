#!/bin/bash
# Seed-2 mutant pool (declared in docs/PREREG_FINAL.md addendum 2026-11-02 (b)); same ranges as the seed-1 test.
S=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents
M=$S/mutants; D=$S/mut2
mkdir -p $D/gen $D/bins $D/logs $D/behave $D/scans; cd $D; cp $M/ranges.txt $M/requests.jsonl .
: > pool.tsv
for f in contact_solver island collide_circle collide_polygon time_of_impact; do
  rg=$(grep -v '^#' ranges.txt | awk -v n=b2_$f '{b=$1; sub(/.*\//,"",b); sub(/\.cpp$/,"",b); if(b==n) print $2}')
  mkdir -p gen/b2_$f
  $S/rc_mutgen --file $M/orig/b2_$f.cpp --ranges $rg --count 100000 --seed 2 --order 1 --out gen/b2_$f > logs/gen_$f.log 2>&1
  echo $f $(ls gen/b2_$f | wc -l)
done
