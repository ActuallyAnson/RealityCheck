# rc_scan: point it at your own simulator

No model card to write, no API key, no model. One command:

    rc_scan --cmd ./mysim --param gain:0.1:8:log --param capacity:5:60:int --param inflow:0:8 --choice mode:fast,safe --steps 200 --out scan_out

Your program only has to read one JSON line per request on stdin and answer with one JSON line `{"ok":true,"signals":{"name":[... steps+1 numbers ...]}}` (`docs/PROTOCOL.md`, `docs/GENERIC_DOMAIN.md`; a 40-line example is `examples/generic/tank.cpp`). You give each parameter's range once.

What it does:
1. Probes the program once to learn its signal names, and builds a model card from your ranges (saved as `scan_out/card.json`, so every other tool works on it afterwards).
2. Runs 200 ordinary scenarios and mines candidate invariants from them (bounds, conserved quantities, monotone signals, step limits; `docs/MINING.md`).
3. Runs N scenarios (default 400, half of them with parameters pushed to the edge of their range, which is where seeded bugs tended to trigger in `docs/HYBRID.md`) and looks for: a failed run, a non-finite signal, nondeterminism (the same scenario twice), and a run that breaks a mined invariant.
4. Minimizes each finding by resetting parameters to their defaults while it still happens, then says what the finding depends on (every parameter is tried at its range ends, its default and every choice: "gain = 8 (it disappears at 0.1, 0.89)"), and writes `scan_out/report.md` and a self-contained `scan_out/report.html` (a plot of every signal of the smallest example, the learned limit dashed, no JavaScript, light and dark). Exit code 1 if anything was found, 0 if not, 2 for a usage or program error.

On the water-tank example (`scripts/scan_selftest.sh`) it takes well under a second and flags the run whose level goes far beyond anything seen in normal runs, with a one-line minimal case.

## First test on a real third-party engine (Box2D, `a292912`)

`examples/external/box2d_scan.cpp` wraps an unmodified Box2D v3 checkout (a closed box of circles; parameters: ball count, radius, restitution, friction, gravity, launch speed, dt, substeps, continuous collision on/off; built with `scripts/build_box2d_hist.sh CLONE a292912 OUT examples/external/box2d_scan.cpp`). No expected answer is encoded. Results, both run once, unselected:
- Continuous collision allowed to be off (400 scenarios): rc_scan flagged three learned-invariant breaks, all the same event: a ball ends up to 1.9 km below the floor of the closed box at high launch speed and large dt. I checked it by hand: with `continuous=1` the same scenarios stay in the box (lowest ball bottom within 0.06 m of the floor); with `continuous=0` they tunnel through the 2 m thick floor. That is Box2D's documented behavior when continuous collision is disabled, so **this is not a Box2D bug**. It is a real "balls leave the arena" event that a user may or may not want to know about.
- Continuous collision forced on (1000 scenarios): **no finding**.
- A flaw in rc_scan found by this test: the report listed only parameters that differ from their default, and `continuous=0` was the default, so the cause was invisible. The report now also lists every parameter of each smallest example.
So on this engine the tool found nothing that is a bug in Box2D. One test on one engine says little either way.

## Second real engine: REBOUND (`262a9cce`), and what it did not find
`examples/external/rebound_scan.cpp` wraps an unmodified REBOUND: 2 to 6 bodies placed from the scenario seed; parameters: body count, mass ratio, spread, speed, softening, dt, integrator (ias15, whfast, leapfrog, bs) and a layout choice (`random`, or `equal_masses_zero_momentum`, where the last body cancels the others' momentum). Reports are in `results/scan/` (`scripts/scan_real.sh` regenerates them from built adapters).
- `random` layout (600 scenarios): one finding, "momentum <= 103.6" for a start with a heavy body at high speed. That is an ordinary large momentum of a valid input, not a bug: a **false positive**. The adapter should have reported momentum drift instead of momentum, a choice that was mine, and the tool cannot make it for you.
- `equal_masses_zero_momentum` (600 scenarios): six findings. The shared one is an energy or radius blow-up with `whfast` or `leapfrog`, softening 0, close bodies and a coarse dt. That is the known behavior of fixed-step integrators on close encounters; I did not investigate beyond what the report's reason line says. Not bugs I can claim.
- **The IAS15 hang from `docs/EXTERNAL_ENGINE.md` was not found.** That hang needs near-exact symmetry (shifting one body by 1e-10 already avoids it) and randomly placed bodies are never that symmetric. I did not tune the adapter until it found the hang, because that would hard-code the answer. So: the tool as a generic scanner does not reproduce the one real finding the orbit-domain harness made. A symmetric start family would find it, but only someone who suspects symmetry would add one.

## Relative rules for signals that follow the start (added after the tests above)
The false positive above came from absolute bounds on a signal whose value at step 0 already differs a lot between runs. For such signals (start spread above a quarter of the signal's overall range) `rc_scan` now drops the absolute bound and learns "stays within R x (|start| + 5% of its scale) of its own start", R = twice the largest ratio seen in the normal runs, at least 1e-6 (below that it is floating-point noise). `--absolute-bounds` restores the old behavior. The shared miner (`src/spec/miner.cpp`) is unchanged, so the numbers in `docs/MINING.md` stand. Re-running the same four scans (`results/scan/` now holds the new reports):
- REBOUND random layout: the momentum false positive is gone (no finding). REBOUND symmetric layout: the momentum finding is gone, five whfast/leapfrog blow-up findings remain.
- Box2D continuous collision free: same tunneling event, now phrased relative to the start.
- **Box2D continuous collision on: one new false positive** ("max_abs_x stays within 4.9x of its start"; a ball launched fast crosses the box, which is valid, and the 200 normal runs did not contain a crossing that large).
- Water tank: the planted flaw is still found.
Net: one false positive removed, one added. The choices (factor 2, floor 1e-6, quarter-of-range) were made after looking at these same scans, so this is not an independent test, and the rule is not shown to be better. It is kept because it removes the false positives that depend on the starting conditions, at the price of a looser rule on signals that move a lot in valid runs.

Honest summary of the real-engine tests (before the relative rules): no bug in either engine, one false positive, one documented behavior reported, and one known bug missed.

Limits, stated plainly: the learned invariants are candidates from the program's own behavior, so a bug that is in every run looks normal to them and a clean scan is not a certificate; a finding may be an unusual but valid input rather than a bug, and only you can tell. Add your own rules to `card.json`, run `rc_agent --live` for a model-driven hunt, or `rc_spec --live` for model-written rules. It has not yet been tried by anyone but me.
