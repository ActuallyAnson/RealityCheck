# Scan of M40_A

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 13 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| learned guess | breaks learned invariant: max_abs_x <= 206.561 (mined) | 2 of 600 | steps=111, gravity=2.12022, speed=59.6961, dt=0.0451358 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: max_abs_x <= 206.561 (mined): steps=111, seed=233993, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=2.12022, speed=59.6961, dt=0.0451358, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0

Why, per finding:

- breaks learned invariant: max_abs_x <= 206.561 (mined): max_abs_x <= 206.561 (mined) (observed 206.702835 vs limit 206.561062 at step 101). It depends on: balls = 15 or 30 (it disappears at 1); gravity = 0 or 2.12022 (it disappears at 15, 30); speed = 60 or 59.6961 (it disappears at 0, 30); dt = 0.05 or 0.0451358 (it disappears at 0.004, 0.027); shape = 0 or 1 (it disappears at 2); joints = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M40_A/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
