# Scan of N19

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 13 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| your rule | breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [min_clearance] | 3 of 600 | steps=20, balls=30, size=0.959611, restitution=0.824031, gravity=18.4369, speed=51.6055, dt=0.0396877, viter=1, joints=1, bullet=1, ccd=1 |
| your rule | breaks relation: shift=0|shift=7 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.01 of the larger value) [max_speed] | 1 of 600 | steps=55, balls=28, size=0.82181, restitution=0.932051, friction=0.537409, speed=36.2751, dt=0.0247492, viter=10, shape=0, bullet=1, ccd=1 |
| learned guess | breaks learned invariant: n_outside <= 12 (mined) | 3 of 600 | steps=24, balls=30, size=0.1, speed=60, dt=0.0395189, viter=1 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [min_clearance]: steps=20, seed=231745, balls=30, size=0.959611, restitution=0.824031, friction=0.5, density=1, gravity=18.4369, speed=51.6055, dt=0.0396877, viter=1, piter=3, sleep=0, shape=1, joints=1, bullet=1, ccd=1, mirror=0, shift=0
- breaks relation: shift=0|shift=7 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.01 of the larger value) [max_speed]: steps=55, seed=597232, balls=28, size=0.82181, restitution=0.932051, friction=0.537409, density=1, gravity=15, speed=36.2751, dt=0.0247492, viter=10, piter=3, sleep=0, shape=0, joints=0, bullet=1, ccd=1, mirror=0, shift=0
- breaks learned invariant: n_outside <= 12 (mined): steps=24, seed=679354, balls=30, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=15, speed=60, dt=0.0395189, viter=1, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0, mirror=0, shift=0

Why, per finding:

- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [min_clearance]: min_clearance is 1.20213 in one run and 0.633234 in the other at step 1. It depends on: balls = 30 (it disappears at 1, 15); size = 0.959611 (it disappears at 0.1, 0.65, 1.2); restitution = 0.5 or 0.824031 (it disappears at 0, 1); gravity = 30 or 18.4369 (it disappears at 0, 15); speed = 60 or 51.6055 (it disappears at 0, 30); dt = 0.004 or 0.0396877 (it disappears at 0.027, 0.05); viter = 1 (it disappears at 6, 12); piter = 3 or 6 (it disappears at 1); shape = 1 (it disappears at 0, 2); joints = 1 (it disappears at 0); bullet = 1 (it disappears at 0); ccd = 1 (it disappears at 0).
- breaks relation: shift=0|shift=7 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.01 of the larger value) [max_speed]: max_speed is 47.9607 in one run and 46.6072 in the other at step 2. It depends on: balls = 30 or 28 (it disappears at 1, 15); size = 0.82181 (it disappears at 0.1, 0.65, 1.2); restitution = 0.932051 (it disappears at 0, 0.5, 1); friction = 0.537409 (it disappears at 0, 0.5, 1); gravity = 15 (it disappears at 0, 30); speed = 36.2751 (it disappears at 0, 30, 60); dt = 0.0247492 (it disappears at 0.004, 0.027, 0.05); viter = 10 (it disappears at 1, 6, 12); shape = 0 (it disappears at 1, 2); joints = 0 (it disappears at 1); bullet = 1 (it disappears at 0); ccd = 1 (it disappears at 0).
- breaks learned invariant: n_outside <= 12 (mined): n_outside <= 12 (mined) (observed 14.000000 vs limit 12.000000 at step 7). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 (it disappears at 0.65, 1.2); speed = 60 (it disappears at 0, 30); dt = 0.05 or 0.0395189 (it disappears at 0.004, 0.027); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=scans/N19/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
