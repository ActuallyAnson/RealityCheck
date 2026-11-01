# Scan of M05_B

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 14 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| your rule | breaks learned invariant: required by you: n_outside <= 0 | 152 of 600 | steps=65, restitution=0.370846, friction=0.52852, density=0.1, piter=1 |
| learned guess | breaks learned invariant: n_outside <= 13.5 (mined) | 5 of 600 | steps=24, balls=30, size=0.1, speed=60, dt=0.0395189, viter=1 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: required by you: n_outside <= 0: steps=65, seed=891753, balls=15, size=0.65, restitution=0.370846, friction=0.52852, density=0.1, gravity=15, speed=30, dt=0.027, viter=6, piter=1, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: n_outside <= 13.5 (mined): steps=24, seed=679354, balls=30, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=15, speed=60, dt=0.0395189, viter=1, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0

Why, per finding:

- breaks learned invariant: required by you: n_outside <= 0: required by you: n_outside <= 0 (observed 1.000000 vs limit 0.000000 at step 14). It depends on: balls = 15 or 30 (it disappears at 1); density = 0.1 (it disappears at 1, 10); dt = 0.027 or 0.05 (it disappears at 0.004); shape = 1 or 2 (it disappears at 0).
- breaks learned invariant: n_outside <= 13.5 (mined): n_outside <= 13.5 (mined) (observed 17.000000 vs limit 13.500000 at step 6). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 (it disappears at 0.65, 1.2); speed = 30 or 60 (it disappears at 0); dt = 0.027 or 0.05 or 0.0395189 (it disappears at 0.004); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M05_B/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
