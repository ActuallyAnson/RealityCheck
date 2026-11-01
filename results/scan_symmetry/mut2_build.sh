#!/bin/bash
# Walk order.tsv (seed-2 shuffle); take sites in order, skip (and log) those that fail to compile, until 40 compiled. Uses the private Box2D tree/build of the seed-1 test (restored after each mutant).
S=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents
M=$S/mutants; D=$S/mut2; R=/Users/anson/Documents/GitHub/RealityCheck
cd $D
: > selected.tsv; : > compile_failures.tsv
k=0; pos=0
while [ $k -lt 40 ]; do
  pos=$((pos+1))
  row=$(sed -n "${pos}p" order.tsv); [ -z "$row" ] && break
  n=$(echo "$row" | cut -f1); mid=$(echo "$row" | cut -f2)
  rel=$(grep -v '^#' ranges.txt | awk -v n=$n '{b=$1; sub(/.*\//,"",b); sub(/\.cpp$/,"",b); if(b==n) print $1}')
  cp gen/$n/$mid.cpp $M/tree/src/$rel
  if (cd $M && nice cmake --build build -j1) >logs/pos$pos.log 2>&1; then
    k=$((k+1)); id=$(printf 'N%02d' $k)
    nice c++ -O2 -std=c++17 -I $M/tree/include -I $R/src $R/examples/external/box2d24_scan.cpp $M/build/bin/libbox2d.a -o bins/$id 2>>logs/pos$pos.log
    printf '%s\t%s\t%s\n' "$id" "$pos" "$row" >> selected.tsv
  else
    printf '%s\t%s\n' "$pos" "$row" >> compile_failures.tsv
  fi
  cp $M/orig/$n.cpp $M/tree/src/$rel
done
(cd $M && nice cmake --build build -j1) >logs/final_restore.log 2>&1
echo BUILD_DONE >> logs/final_restore.log
