#!/bin/sh
# Writes web/data/gap.json (the viewer's "Oracle gap" screen) from the stored result files; no model, no simulation.
cd "$(dirname "$0")/.."
B=${BUILD_DIR:-build}
"$B/rc_gap" rigid2d="${GAP_RIGID2D:--}" orbit=results/reforacle/orbit/mutants cartpole=results/reforacle/cartpole/mutants traffic="${GAP_TRAFFIC:-results/reforacle/traffic/draw2/mutants}" > web/data/gap.json
echo "wrote web/data/gap.json"
