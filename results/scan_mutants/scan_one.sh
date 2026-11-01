#!/bin/bash
# usage: scan_one.sh id arm(A|B)   -> scans/<id>_<arm>/report.md
M=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants
SCAN=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/rc_scan_mut
export RC_EXEC_TIMEOUT_S=10
extra=(); [ "$2" = B ] && extra=(--require "n_outside<=0")
nice $SCAN --cmd $M/bins/$1 --param balls:1:30:int --param size:0.1:1.2 --param restitution:0:1 --param friction:0:1 --param density:0.1:10:log --param gravity:0:30 --param speed:0:60 --param dt:0.004:0.05 --param viter:1:12:int --param piter:1:6:int --param sleep:0:1:int --param shape:0:2:int --param joints:0:1:int --param bullet:0:1:int --param ccd:0:1:int --steps 200 --scenarios 600 "${extra[@]}" --out $M/scans/$1_$2 --name $1_$2 > $M/scans/$1_$2.stdout 2>&1
echo "$1 $2 exit=$?" >> $M/scans/exit_codes.txt
