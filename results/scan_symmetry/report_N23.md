# Scan of N23

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 13 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| learned guess | breaks learned invariant: kinetic stays within 3.53108 x (|its start| + 58117) of its own start (mined, relative) | 3 of 600 | steps=96, size=1.083, density=10, speed=0, shape=0 |
| learned guess | breaks learned invariant: min_clearance <= 234.827 (mined) | 1 of 600 | steps=185, balls=1, gravity=2.06485, speed=60, shape=2 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: kinetic stays within 3.53108 x (|its start| + 58117) of its own start (mined, relative): steps=96, seed=959510, balls=15, size=1.083, restitution=0.5, friction=0.5, density=10, gravity=15, speed=0, dt=0.027, viter=6, piter=3, sleep=0, shape=0, joints=0, bullet=0, ccd=0, mirror=0, shift=0
- breaks learned invariant: min_clearance <= 234.827 (mined): steps=185, seed=88264, balls=1, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=2.06485, speed=60, dt=0.027, viter=6, piter=3, sleep=0, shape=2, joints=0, bullet=0, ccd=0, mirror=0, shift=0

Why, per finding:

- breaks learned invariant: kinetic stays within 3.53108 x (|its start| + 58117) of its own start (mined, relative): kinetic stays within 3.53108 x (|its start| + 58117) of its own start (mined, relative) (observed 209603.048484 vs limit 0.000000 at step 68). It depends on: balls = 15 or 30 (it disappears at 1); size = 1.2 or 1.083 (it disappears at 0.1, 0.65); density = 10 (it disappears at 0.1, 1); gravity = 15 or 30 (it disappears at 0); speed = 0 (it disappears at 30, 60); dt = 0.027 or 0.05 (it disappears at 0.004); shape = 0 or 2 (it disappears at 1).
- breaks learned invariant: min_clearance <= 234.827 (mined): min_clearance <= 234.827 (mined) (observed 235.462836 vs limit 234.826527 at step 150). It depends on: balls = 1 (it disappears at 15, 30); gravity = 0 or 2.06485 (it disappears at 15, 30); speed = 60 (it disappears at 0, 30); dt = 0.027 or 0.05 (it disappears at 0.004); shape = 0 or 2 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=scans/N23/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
