# rc_scan: point it at your own simulator

No model card to write, no API key, no model. One command:

    rc_scan --cmd ./mysim --param gain:0.1:8:log --param capacity:5:60:int --param inflow:0:8 --choice mode:fast,safe --steps 200 --out scan_out

Your program only has to read one JSON line per request on stdin and answer with one JSON line `{"ok":true,"signals":{"name":[... steps+1 numbers ...]}}` (`docs/PROTOCOL.md`, `docs/GENERIC_DOMAIN.md`; a 40-line example is `examples/generic/tank.cpp`). You give each parameter's range once.

What it does:
1. Probes the program once to learn its signal names, and builds a model card from your ranges (saved as `scan_out/card.json`, so every other tool works on it afterwards).
2. Runs 200 ordinary scenarios and mines candidate invariants from them (bounds, conserved quantities, monotone signals, step limits; `docs/MINING.md`).
3. Runs N scenarios (default 400, half of them with parameters pushed to the edge of their range, which is where seeded bugs tended to trigger in `docs/HYBRID.md`) and looks for: a failed run, a non-finite signal, nondeterminism (the same scenario twice), and a run that breaks a mined invariant.
4. Minimizes each finding by resetting parameters to their defaults while it still happens, and writes `scan_out/report.md`. Exit code 1 if anything was found, 0 if not, 2 for a usage or program error.

On the water-tank example (`scripts/scan_selftest.sh`) it takes well under a second and flags the run whose level goes far beyond anything seen in normal runs, with a one-line minimal case.

## First test on a real third-party engine (Box2D, `a292912`)

`examples/external/box2d_scan.cpp` wraps an unmodified Box2D v3 checkout (a closed box of circles; parameters: ball count, radius, restitution, friction, gravity, launch speed, dt, substeps, continuous collision on/off; built with `scripts/build_box2d_hist.sh CLONE a292912 OUT examples/external/box2d_scan.cpp`). No expected answer is encoded. Results, both run once, unselected:
- Continuous collision allowed to be off (400 scenarios): rc_scan flagged three learned-invariant breaks, all the same event: a ball ends up to 1.9 km below the floor of the closed box at high launch speed and large dt. I checked it by hand: with `continuous=1` the same scenarios stay in the box (lowest ball bottom within 0.06 m of the floor); with `continuous=0` they tunnel through the 2 m thick floor. That is Box2D's documented behavior when continuous collision is disabled, so **this is not a Box2D bug**. It is a real "balls leave the arena" event that a user may or may not want to know about.
- Continuous collision forced on (1000 scenarios): **no finding**.
- A flaw in rc_scan found by this test: the report listed only parameters that differ from their default, and `continuous=0` was the default, so the cause was invisible. The report now also lists every parameter of each smallest example.
So on this engine the tool found nothing that is a bug in Box2D. One test on one engine says little either way.

Limits, stated plainly: the learned invariants are candidates from the program's own behavior, so a bug that is in every run looks normal to them and a clean scan is not a certificate; a finding may be an unusual but valid input rather than a bug, and only you can tell. Add your own rules to `card.json`, run `rc_agent --live` for a model-driven hunt, or `rc_spec --live` for model-written rules. It has not yet been tried by anyone but me.
