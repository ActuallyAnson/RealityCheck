#!/bin/bash
# Re-scan clean, N04 and N39 with the fixed --same (addendum 2026-11-02 (c)). usage: rescan3.sh <rc_scan> <bins-dir-with-clean_sym,N04,N39> <out-dir>
set -u; export RC_EXEC_TIMEOUT_S=10; R=$1; B=$2; O=$3; mkdir -p "$O"
for id in clean N04 N39; do
  bin=$B/$id; [ "$id" = clean ] && bin=$B/clean_sym
  nice "$R" --cmd "$bin" --param balls:1:30:int --param size:0.1:1.2 --param restitution:0:1 --param friction:0:1 --param density:0.1:10:log --param gravity:0:30 --param speed:0:60 --param dt:0.004:0.05 --param viter:1:12:int --param piter:1:6:int --param sleep:0:1:int --param shape:0:2:int --param joints:0:1:int --param bullet:0:1:int --param ccd:0:1:int --param mirror:0:0:int --param shift:0:0 --same "mirror=0|mirror=1:max_abs_x,kinetic,max_speed,min_clearance:first=2:tol=0.2" --same "shift=0|shift=7:max_abs_x,kinetic,max_speed,min_clearance:first=2:tol=0.01" --steps 200 --scenarios 600 --out "$O/$id" --name "$id" > "$O/$id.stdout" 2>&1
  echo "$id exit=$?" >> "$O/exit_codes.txt"
done
echo DONE >> "$O/exit_codes.txt"
