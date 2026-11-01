#!/bin/bash
# usage: run2.sh stream(0|1)   classify (300 requests) then scan; ids split by parity; clean handled by stream 0
S=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents
D=$S/mut2; cd $D; export RC_EXEC_TIMEOUT_S=10
beh() { : > behave/$1.out; while IFS= read -r l; do o=$(printf '%s\n' "$l" | nice timeout 20 "$2" 2>/dev/null); rc=$?; if [ $rc -eq 124 ]; then echo TIMEOUT >> behave/$1.out; elif [ $rc -ne 0 ]; then echo "CRASH $rc" >> behave/$1.out; else echo "$o" >> behave/$1.out; fi; done < requests.jsonl; }
scan() { nice $S/rc_scan_sym --cmd $2 --param balls:1:30:int --param size:0.1:1.2 --param restitution:0:1 --param friction:0:1 --param density:0.1:10:log --param gravity:0:30 --param speed:0:60 --param dt:0.004:0.05 --param viter:1:12:int --param piter:1:6:int --param sleep:0:1:int --param shape:0:2:int --param joints:0:1:int --param bullet:0:1:int --param ccd:0:1:int --param mirror:0:0:int --param shift:0:0 --same "mirror=0|mirror=1:max_abs_x,kinetic,max_speed,min_clearance:first=2:tol=0.2" --same "shift=0|shift=7:max_abs_x,kinetic,max_speed,min_clearance:first=2:tol=0.01" --steps 200 --scenarios 600 --out scans/$1 --name $1 > scans/$1.stdout 2>&1; echo "$1 exit=$?" >> scans/exit_codes.txt; }
if [ "$1" = 0 ]; then beh clean $S/clean_sym; scan clean $S/clean_sym; fi
i=0
for id in $(cut -f1 selected.tsv); do
  i=$((i+1)); [ $((i%2)) -ne "$1" ] && continue
  beh $id bins/$id
  if cmp -s behave/$id.out behave/clean.out 2>/dev/null; then echo "$id unchanged" >> class2.txt; continue; fi
  grep -qE "^(TIMEOUT|CRASH)" behave/$id.out && echo "$id crashed" >> class2.txt || echo "$id changed" >> class2.txt
  scan $id bins/$id
done
echo "STREAM$1 DONE" >> scans/exit_codes.txt
