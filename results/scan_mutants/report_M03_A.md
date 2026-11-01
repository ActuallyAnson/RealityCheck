# Scan of M03_A

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 13 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| learned guess | breaks learned invariant: n_outside <= 12 (mined) | 3 of 600 | steps=24, balls=30, size=0.1, speed=60, dt=0.0395189, viter=1 |
| learned guess | breaks learned invariant: max_speed stays within 9.32377 x (|its start| + 5.07439) of its own start (mined, relative) | 1 of 600 | steps=148, balls=30, size=0.402034, restitution=0.335193, friction=1, gravity=28.5374, speed=0, dt=0.0119832, viter=7, piter=1 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: n_outside <= 12 (mined): steps=24, seed=679354, balls=30, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=15, speed=60, dt=0.0395189, viter=1, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: max_speed stays within 9.32377 x (|its start| + 5.07439) of its own start (mined, relative): steps=148, seed=640666, balls=30, size=0.402034, restitution=0.335193, friction=1, density=1, gravity=28.5374, speed=0, dt=0.0119832, viter=7, piter=1, sleep=0, shape=1, joints=0, bullet=0, ccd=0

Why, per finding:

- breaks learned invariant: n_outside <= 12 (mined): n_outside <= 12 (mined) (observed 14.000000 vs limit 12.000000 at step 7). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 (it disappears at 0.65, 1.2); speed = 60 (it disappears at 0, 30); dt = 0.05 or 0.0395189 (it disappears at 0.004, 0.027); ccd = 0 (it disappears at 1).
- breaks learned invariant: max_speed stays within 9.32377 x (|its start| + 5.07439) of its own start (mined, relative): max_speed stays within 9.32377 x (|its start| + 5.07439) of its own start (mined, relative) (observed 53.091194 vs limit 0.000000 at step 86). It depends on: balls = 30 (it disappears at 1, 15); size = 0.402034 (it disappears at 0.1, 0.65, 1.2); restitution = 0.335193 (it disappears at 0, 0.5, 1); friction = 0 or 1 (it disappears at 0.5); gravity = 28.5374 (it disappears at 0, 15, 30); speed = 0 (it disappears at 30, 60); dt = 0.0119832 (it disappears at 0.004, 0.027, 0.05); viter = 7 (it disappears at 1, 6, 12); piter = 1 (it disappears at 3, 6); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M03_A/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
