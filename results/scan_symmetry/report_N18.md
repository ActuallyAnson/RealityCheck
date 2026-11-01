# Scan of N18

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 13 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| learned guess | breaks learned invariant: n_outside <= 12 (mined) | 3 of 600 | steps=24, balls=30, size=0.1, speed=60, dt=0.0395189, viter=1 |
| learned guess | breaks learned invariant: kinetic stays within 1.89662 x (|its start| + 58117) of its own start (mined, relative) | 1 of 600 | steps=48, balls=30, size=1.083, restitution=1, density=10, speed=0, dt=0.05, viter=1, shape=0 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: n_outside <= 12 (mined): steps=24, seed=679354, balls=30, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=15, speed=60, dt=0.0395189, viter=1, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0, mirror=0, shift=0
- breaks learned invariant: kinetic stays within 1.89662 x (|its start| + 58117) of its own start (mined, relative): steps=48, seed=959510, balls=30, size=1.083, restitution=1, friction=0.5, density=10, gravity=15, speed=0, dt=0.05, viter=1, piter=3, sleep=0, shape=0, joints=0, bullet=0, ccd=0, mirror=0, shift=0

Why, per finding:

- breaks learned invariant: n_outside <= 12 (mined): n_outside <= 12 (mined) (observed 14.000000 vs limit 12.000000 at step 7). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 (it disappears at 0.65, 1.2); speed = 60 (it disappears at 0, 30); dt = 0.05 or 0.0395189 (it disappears at 0.004, 0.027); ccd = 0 (it disappears at 1).
- breaks learned invariant: kinetic stays within 1.89662 x (|its start| + 58117) of its own start (mined, relative): kinetic stays within 1.89662 x (|its start| + 58117) of its own start (mined, relative) (observed 115661.960143 vs limit 0.000000 at step 21). It depends on: balls = 30 (it disappears at 1, 15); size = 1.2 or 1.083 (it disappears at 0.1, 0.65); restitution = 1 (it disappears at 0, 0.5); density = 10 (it disappears at 0.1, 1); gravity = 15 or 30 (it disappears at 0); speed = 0 (it disappears at 30, 60); dt = 0.05 (it disappears at 0.004, 0.027); viter = 1 (it disappears at 6, 12); shape = 0 or 2 (it disappears at 1); joints = 0 (it disappears at 1); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=scans/N18/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
