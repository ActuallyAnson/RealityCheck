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

Limits, stated plainly: the learned invariants are candidates from the program's own behavior, so a bug that is in every run looks normal to them and a clean scan is not a certificate; a finding may be an unusual but valid input rather than a bug, and only you can tell. Add your own rules to `card.json`, run `rc_agent --live` for a model-driven hunt, or `rc_spec --live` for model-written rules. It has not yet been tried by anyone but me.
