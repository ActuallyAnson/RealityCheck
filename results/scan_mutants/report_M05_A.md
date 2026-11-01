# Scan of M05_A

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 13 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| learned guess | breaks learned invariant: n_outside <= 13.5 (mined) | 5 of 600 | steps=24, balls=30, size=0.1, speed=60, dt=0.0395189, viter=1 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: n_outside <= 13.5 (mined): steps=24, seed=679354, balls=30, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=15, speed=60, dt=0.0395189, viter=1, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0

Why, per finding:

- breaks learned invariant: n_outside <= 13.5 (mined): n_outside <= 13.5 (mined) (observed 17.000000 vs limit 13.500000 at step 6). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 (it disappears at 0.65, 1.2); speed = 30 or 60 (it disappears at 0); dt = 0.027 or 0.05 or 0.0395189 (it disappears at 0.004); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M05_A/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
