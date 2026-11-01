# Scan of N01

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 13 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| your rule | breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [max_speed] | 2 of 600 | steps=75, balls=25, piter=6 |
| learned guess | breaks learned invariant: n_outside <= 12 (mined) | 3 of 600 | steps=24, balls=30, size=0.1, speed=60, dt=0.0395189, viter=1 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [max_speed]: steps=75, seed=723866, balls=25, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=6, sleep=0, shape=1, joints=0, bullet=0, ccd=0, mirror=0, shift=0
- breaks learned invariant: n_outside <= 12 (mined): steps=24, seed=679354, balls=30, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=15, speed=60, dt=0.0395189, viter=1, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0, mirror=0, shift=0

Why, per finding:

- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [max_speed]: max_speed is 62.4629 in one run and 36.0317 in the other at step 2. It depends on: balls = 30 or 25 (it disappears at 1, 15); size = 0.65 or 1.2 (it disappears at 0.1); friction = 0.5 or 1 (it disappears at 0); speed = 30 (it disappears at 0, 60); dt = 0.004 or 0.027 (it disappears at 0.05); viter = 6 or 12 (it disappears at 1); piter = 6 (it disappears at 1, 3); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1).
- breaks learned invariant: n_outside <= 12 (mined): n_outside <= 12 (mined) (observed 14.000000 vs limit 12.000000 at step 7). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 (it disappears at 0.65, 1.2); speed = 60 (it disappears at 0, 30); dt = 0.05 or 0.0395189 (it disappears at 0.004, 0.027); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=scans/N01/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
