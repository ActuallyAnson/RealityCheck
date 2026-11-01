# Scan of M21_B

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 15 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| your rule | breaks learned invariant: required by you: n_outside <= 0 | 172 of 600 | steps=66, size=0.352064 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: required by you: n_outside <= 0: steps=66, seed=496328, balls=15, size=0.352064, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0

Why, per finding:

- breaks learned invariant: required by you: n_outside <= 0: required by you: n_outside <= 0 (observed 1.000000 vs limit 0.000000 at step 7). It depends on: dt = 0.027 or 0.05 (it disappears at 0.004); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M21_B/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
