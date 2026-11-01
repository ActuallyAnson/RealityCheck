# Scan of N40

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 13 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| your rule | breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [min_clearance] | 1 of 600 | steps=54, size=0.914107, shape=2 |
| learned guess | breaks learned invariant: max_speed stays within 10.6295 x (|its start| + 4.44737) of its own start (mined, relative) | 1 of 600 | steps=70, balls=22, size=0.364599, restitution=0.218688, friction=1, gravity=30, speed=0, dt=0.0394869, shape=2 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [min_clearance]: steps=54, seed=297994, balls=15, size=0.914107, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=2, joints=0, bullet=0, ccd=0, mirror=0, shift=0
- breaks learned invariant: max_speed stays within 10.6295 x (|its start| + 4.44737) of its own start (mined, relative): steps=70, seed=786901, balls=22, size=0.364599, restitution=0.218688, friction=1, density=1, gravity=30, speed=0, dt=0.0394869, viter=6, piter=3, sleep=0, shape=2, joints=0, bullet=0, ccd=0, mirror=0, shift=0

Why, per finding:

- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [min_clearance]: min_clearance is 2.47053 in one run and 1.71177 in the other at step 1. It depends on: balls = 15 (it disappears at 1, 30); size = 1.2 or 0.914107 (it disappears at 0.1, 0.65); restitution = 0.5 or 1 (it disappears at 0); speed = 30 or 60 (it disappears at 0); dt = 0.027 or 0.05 (it disappears at 0.004); shape = 2 (it disappears at 0, 1).
- breaks learned invariant: max_speed stays within 10.6295 x (|its start| + 4.44737) of its own start (mined, relative): max_speed stays within 10.6295 x (|its start| + 4.44737) of its own start (mined, relative) (observed 47.384228 vs limit 0.000000 at step 40). It depends on: balls = 30 or 22 (it disappears at 1, 15); size = 0.1 or 0.65 or 0.364599 (it disappears at 1.2); gravity = 30 (it disappears at 0, 15); speed = 0 (it disappears at 30, 60); dt = 0.0394869 (it disappears at 0.004, 0.027, 0.05); viter = 6 or 12 (it disappears at 1); shape = 2 (it disappears at 0, 1); joints = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=scans/N40/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
