#!/bin/bash
S=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents
cd $S/sym; export RC_EXEC_TIMEOUT_S=10
A=()
for r in "mirror=0|mirror=1" "shift=0|shift=7"; do for f in 2 5 10; do for t in 0.01 0.05 0.2; do A+=(--same "$r:max_abs_x,kinetic,max_speed,min_clearance:first=$f:tol=$t"); done; done; done
nice $S/rc_scan_sym --cmd $S/clean_sym --param balls:1:30:int --param size:0.1:1.2 --param restitution:0:1 --param friction:0:1 --param density:0.1:10:log --param gravity:0:30 --param speed:0:60 --param dt:0.004:0.05 --param viter:1:12:int --param piter:1:6:int --param sleep:0:1:int --param shape:0:2:int --param joints:0:1:int --param bullet:0:1:int --param ccd:0:1:int --param mirror:0:0:int --param shift:0:0 "${A[@]}" --steps 200 --scenarios 600 --out grid --name grid > grid.stdout 2>&1
