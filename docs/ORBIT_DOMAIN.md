# Second domain: orbits (softened N-body gravity)

Same tools, rules engine, shrinker and agent as the rigid-body domain; only the simulator, rule catalogue,
`inspect_state`/`render_frames` output and the prompt's simulation description change (`--domain orbit`).
The rigid2d prompt, tool schemas, scenario hashes and trace hashes are pinned by a test, so adding the domain
did not change any rigid2d result or replay cache.

Seeded bugs (hidden from the tester): `o01`..`o03`, each only active under a condition, one per rule family
(energy, momentum, angular momentum). Their descriptions live in `data/ground_truth.json` (harness only).

Calibration, random baseline only, before any agent run (`rc_orbit_cal`, `rc_eval baseline`):
- clean simulator: 0 flagged in 20000 random scenarios;
- random baseline: each bug found 5/5 seeds, median 7 to 12 scenarios, 0 false positives on 3 controls.
So these bugs are easy for random testing. They show the tester generalises, not that the agent beats random here.

Agent evaluation (`data/eval_lock_orbit.json`, 5 seeds per bug plus 3 controls, unchanged v1 agent):

| bug | detected | valid and attributable | explanation names cause | median scenarios to first violation |
|---|---|---|---|---|
| o01 | 5/5 | 5/5 | 4/5 | 3 |
| o02 | 5/5 | 5/5 | 5/5 | 4 |
| o03 | 5/5 | 5/5 | 5/5 | 2 |

Controls: 0 reports in 3 runs. Cost $1.06 for 18 runs. Full table: `results/orbit_agent/summary.md`.

Disclosures:
- A first attempt was stopped after 3 runs (about $0.14) because reports could not cite whole-system quantities
  (angular momentum, momentum). The validator was extended (harness side) and all 18 runs were redone from scratch;
  the agent, prompts and lock did not change.
- "Explanation names cause" is the same transparent keyword rule as the rigid2d evaluation; fixed before the runs.
- The bugs were planted by the author, and 15 runs per domain is a small sample.
