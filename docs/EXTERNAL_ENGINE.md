# Testing an outside engine (Box2D 3.x)

`rc_box2d` runs random scenarios (no model, no API) through an **unmodified Box2D** world and checks the same
declared rules. The adapter is `src/external/box2d_sim.cpp`; walls get restitution 0 so bodies decide the bounce.
Controller scenarios are off (Box2D has no controller), so `controller_force_limit` cannot fire.
Reproduce: `./build/rc_box2d --scenarios 3000 [--no-continuous] --out results/box2d_*.json`.

| Configuration | in_arena | no_wall_penetration | no_tunneling | energy_bound |
|---|---|---|---|---|
| Box2D default (continuous on), 3000 scenarios | 889 | 199 | 0 | 0 |
| continuous collision off, 3000 scenarios | 2088 | 851 | 186 | 0 |

How to read it (these are not claims of new engine bugs):
- **Penetration flags with default settings** come from Box2D's soft contacts: a fast body sinks past our 0.15
  tolerance before being pushed out. That is a documented design trade-off, so it shows our declared tolerance,
  not an engine defect. The rule thresholds are ours.
- **Tunneling with continuous collision off** matches the known class of reports (e.g. forum threads about
  tunneling at low frame rates) and is the documented reason continuous collision exists. It is a
  misconfiguration check, not a newly found bug.
- No energy gain was flagged in either configuration.

## Model-driven agent on Box2D (one run per configuration, live API, ~$0.03 and ~$0.05)

`rc_agent --live --engine box2d|box2d-noccd` (reports in `results/box2d_agent/`). There is no ground truth for an
outside engine, so only two things are checked: the violation is real under the declared rules (reproduced by the
deterministic checker after shrinking) and every cited state matches the recorded run.
- `box2d-noccd`: valid report, `no_wall_penetration`, shrunk to a 60 m/s body at a 0.1 m wall. The model's
  *cause* (substep count too low) is a hypothesis nobody verified here.
- `box2d` (defaults): valid report, `in_arena`, from a 100:1 mass-ratio head-on squeeze against the boundary.
  The cause given (solver overlap in a sandwich) is again unverified. It is a rule-tolerance flag, not a
  confirmed engine defect.
Two runs are an anecdote, not an evaluation.

Environment: Box2D 3.2.0 from Homebrew on macOS (arm64); counts depend on the version.

Not used: Chipmunk2D 7.0.3 (Homebrew bottle). On this machine `cpSpaceStep` never returns once a static shape exists,
even in a 10-line program (stuck in `FloodFillComponent`), so no results were taken from it. Not investigated further.

## Tooling since the first version
- `rc_box2d` became `rc_hunt --engine box2d|box2d-noccd` (same numbers: 889/199 flagged with continuous collision on,
  checked after the refactor). `rc_hunt` works for any engine spec and writes a regression bundle per rule (`--bundles`).
- `rc_repro export|check` turns a found failure (agent report or hunt) into `scenario.json` + `expected.json` +
  `test.sh`; exit code 1 means the failure reproduces, 0 that the rule holds, with `--junit` for CI.
- `rc_diff` compares two engines on the same scenarios (no rules needed). Findings, all with the built-in orbit
  simulator as one side, tolerance 0.1, first 1 s of each run, 300 random scenarios:
  - independent fixed-step integrator, 0.004 s substep: 0 disagreements (the two implementations agree);
  - same integrator, 0.03 s substep: 28 disagreements;
  - the seeded `o02` bug versus clean: 5 disagreements (the rule-based check finds it in about 26 of 400; the oracle
    needs no rules but is less sensitive);
  - limit: contact-dynamics systems are chaotic, so two different rigid-body engines disagree in most scenarios
    (built-in vs Box2D: 359 of 400 at tolerance 0.5), which is noise, not bugs. Differential testing needs engines that
    are numerically close, a short horizon, or a quantity that is not chaotic.

## REBOUND (N-body library), orbit domain

Target: REBOUND 5.2.2 (commit 262a9cc, a widely used open-source N-body code, GPLv3), unmodified, built with
`scripts/build_rebound_example.sh` and driven through `examples/external/rebound_orbit.cpp` over the JSON-lines protocol. The adapter uses REBOUND's own
direct-summation gravity with Plummer softening 0.3, the force law the orbit domain declares. Nothing of REBOUND is copied into this repository.
Raw output: `results/rebound/hunt.txt`. 1000 random scenarios per row, the orbit domain's declared rules (energy, momentum, angular momentum conserved).

| REBOUND integrator (substeps per scenario dt) | scenarios with a rule violation | rule |
|---|---|---|
| IAS15 (adaptive, 15th order) | 0 / 1000 | |
| BS (Bulirsch-Stoer, adaptive) | 0 / 1000 | |
| leapfrog, 1 substep | 172 / 1000 | energy_conserved |
| leapfrog, 4 substeps | 1 / 1000 | energy_conserved |
| leapfrog, 16 substeps | 0 / 1000 | |
| SABA, 1 substep | 188 / 1000 | energy_conserved |
| built-in simulator (reference) | 0 / 1000 | |

Differential check with `rc_diff` against the built-in simulator (100 scenarios, tolerance 0.1, horizon 1 s): IAS15 0, BS 0, leapfrog with 8 substeps 0,
leapfrog with 1 substep 9.

**Reading it correctly:** these are not bugs in REBOUND. A fixed-step second-order integrator run at the scenario's outer step (up to 1/30 s)
violates a 2% energy bound in close encounters, and SABA (built for a dominant central mass) also prints convergence warnings on these equal-mass,
softened systems. What the run shows is that the tool, with no knowledge of REBOUND, (1) agrees with the library's high-accuracy integrators, (2) isolates
the step-size dependence of its fixed-step ones with shrunk reproductions (two bodies for the 1-substep leapfrog and SABA, 8 for the 4-substep leapfrog), and (3) works against a real C library written by someone else, through the
same pipe protocol as the sample simulator. Momentum and angular momentum stayed within the declared tolerances in every integrator and scenario. No defect in REBOUND is claimed.

### Externally sourced scenarios, and a hang in REBOUND's IAS15

`data/external_scenarios/` holds 14 scenarios from the literature and standard practice (figure-eight, Pythagorean and Lagrange three-body problems, Kepler orbits,
Newton's cradle, a billiard break, the classic cart-pole constants, ...; see its README). `./build/rc_corpus` runs them on the built-in simulator, on unmodified
Box2D (both collision settings), and on REBOUND's IAS15 (adaptive_mode 0), BS and leapfrog (1 and 4 substeps): **every scenario passes every rule on every engine**
(`results/rebound/corpus.md`). The tool's verdicts agree with what the literature predicts for well-behaved inputs; these are not failures.

One scenario did expose a real problem. **REBOUND 5.2.2 (commit 262a9cc), IAS15 with its default settings, never returns from a single `reb_simulation_integrate(r, 0.01)` on the figure-eight
initial condition.** The standalone C reproduction `examples/external/rebound_ias15_hang.c` has no RealityCheck code, only REBOUND's public API. What I checked:
the hang does not depend on softening (0, 0.1, 0.3, 1.0), on the position offset or on the initial step (0.001 to 0.1); with `adaptive_mode` 0 (INDIVIDUAL) or 1 (GLOBAL) it finishes at
once, and only the default mode 2 (PRS23, the default since January 2024) hangs; BS and leapfrog on the same input are fine. A web search found no report of it.
What I did not check: other REBOUND versions, why PRS23 loops (I read none of its code), or whether the exact symmetry of the initial condition is the trigger (a 1e-9 shift of one body made
the full adapter finish but not the standalone file, so I do not claim a trigger). It may be a configuration subtlety on my side, so I call it a reproducible hang found through the corpus,
not a confirmed REBOUND bug, and I have not reported it upstream. The tool found it because the exec protocol made a non-returning simulator visible; a per-request timeout in `exec_sim` is the missing
safeguard (a hung engine currently blocks the run).
