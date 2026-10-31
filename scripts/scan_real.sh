#!/bin/sh
# Runs rc_scan on the two third-party engines of docs/SCAN.md and stores the reports in results/scan/ (paths to the already built adapters are arguments;
# build them with scripts/build_box2d_hist.sh ... examples/external/box2d_scan.cpp and scripts/build_rebound_example.sh-style object files).
# usage: scripts/scan_real.sh BUILD_DIR BOX2D_SCAN_BIN REBOUND_SCAN_BIN
cd "$(dirname "$0")/.."
B="${1:?build dir}"; BX="${2:?box2d_scan binary}"; RB="${3:?rebound_scan binary}"
O=results/scan; mkdir -p "$O"
export RC_EXEC_TIMEOUT_S=10
clean() { sed -e '/^Re-run with the saved card/d' "$1/report.md" > "$2"; }
T=$(mktemp -d)
nice "$B/rc_scan" --cmd "$BX" --param balls:1:30:int --param radius:0.1:1.5 --param restitution:0:1 --param friction:0:1 --param gravity:0:30 --param speed:0:60 --param dt:0.004:0.05 --param substeps:1:8:int --param continuous:0:1:int --steps 200 --scenarios 400 --out "$T/a" --name box2d-continuous-allowed-off > /dev/null 2>&1; clean "$T/a" "$O/box2d_ccd_free.md"
nice "$B/rc_scan" --cmd "$BX" --param balls:1:40:int --param radius:0.05:1.5 --param restitution:0:1 --param friction:0:1 --param gravity:0:30 --param speed:0:60 --param dt:0.004:0.05 --param substeps:1:8:int --param continuous:1:1:int --steps 300 --scenarios 1000 --out "$T/b" --name box2d-continuous-on > /dev/null 2>&1; clean "$T/b" "$O/box2d_ccd_on.md"
for lay in "random,random_again:random" "random,equal_masses_zero_momentum:symmetric"; do
  nice "$B/rc_scan" --cmd "$RB" --param bodies:2:6:int --param mass_ratio:1:100:log --param spread:2:20 --param speed:0:2 --param softening:0:1 --param dt:0.001:0.05:log --choice integrator:ias15,whfast,leapfrog,bs --steps 50 --scenarios 600 --choice layout:"${lay%%:*}" --out "$T/r" --name "rebound-${lay##*:}" > /dev/null 2>&1; clean "$T/r" "$O/rebound_${lay##*:}.md"; rm -rf "$T/r"
done
rm -rf "$T"
echo "wrote $O"
