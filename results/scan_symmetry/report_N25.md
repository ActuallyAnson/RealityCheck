# Scan of N25

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 13 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| your rule | breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [max_speed] | 2 of 600 | steps=23, size=0.374964, friction=0, speed=11.6137, shape=2 |
| your rule | breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [kinetic] | 1 of 600 | steps=23, size=0.374964, friction=0, speed=11.6137, shape=2 |
| learned guess | breaks learned invariant: n_outside <= 12 (mined) | 3 of 600 | steps=24, balls=30, size=0.1, speed=60, dt=0.0395189, viter=1 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [max_speed]: steps=23, seed=540146, balls=15, size=0.374964, restitution=0.5, friction=0, density=1, gravity=15, speed=11.6137, dt=0.027, viter=6, piter=3, sleep=0, shape=2, joints=0, bullet=0, ccd=0, mirror=0, shift=0
- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [kinetic]: steps=23, seed=540146, balls=15, size=0.374964, restitution=0.5, friction=0, density=1, gravity=15, speed=11.6137, dt=0.027, viter=6, piter=3, sleep=0, shape=2, joints=0, bullet=0, ccd=0, mirror=0, shift=0
- breaks learned invariant: n_outside <= 12 (mined): steps=24, seed=679354, balls=30, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=15, speed=60, dt=0.0395189, viter=1, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0, mirror=0, shift=0

Why, per finding:

- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [max_speed]: max_speed is 30.8109 in one run and 12.406 in the other at step 2. It depends on: balls = 15 or 30 (it disappears at 1); size = 0.374964 (it disappears at 0.1, 0.65, 1.2); restitution = 0.5 (it disappears at 0, 1); friction = 0 (it disappears at 0.5, 1); speed = 11.6137 (it disappears at 0, 30, 60); dt = 0.027 (it disappears at 0.004, 0.05); shape = 2 (it disappears at 0, 1); joints = 0 (it disappears at 1).
- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [kinetic]: kinetic is 654.743 in one run and 444.687 in the other at step 2. It depends on: balls = 15 (it disappears at 1, 30); size = 0.374964 (it disappears at 0.1, 0.65, 1.2); restitution = 0.5 (it disappears at 0, 1); friction = 0 (it disappears at 0.5, 1); speed = 11.6137 (it disappears at 0, 30, 60); dt = 0.027 (it disappears at 0.004, 0.05); shape = 2 (it disappears at 0, 1); joints = 0 (it disappears at 1).
- breaks learned invariant: n_outside <= 12 (mined): n_outside <= 12 (mined) (observed 14.000000 vs limit 12.000000 at step 7). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 (it disappears at 0.65, 1.2); speed = 60 (it disappears at 0, 30); dt = 0.05 or 0.0395189 (it disappears at 0.004, 0.027); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=scans/N25/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
