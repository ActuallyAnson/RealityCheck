# Scan of N09

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 12 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| your rule | breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [min_clearance] | 1 of 600 | steps=57, balls=28, size=0.854059, restitution=0, dt=0.0489361, viter=1, piter=6, bullet=1, ccd=1 |
| learned guess | breaks learned invariant: kinetic stays within 31.5766 x (|its start| + 189236) of its own start (mined, relative) | 1 of 600 | steps=197, size=1.01284, density=9.46086, gravity=22.5536, speed=20.2354, dt=0.0477534 |
| learned guess | breaks learned invariant: max_abs_x stays within 3.73355 x (|its start| + 0.779318) of its own start (mined, relative) | 1 of 600 | steps=49, size=0.1, gravity=0, speed=60, dt=0.0395189, viter=1, shape=0, joints=1 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [min_clearance]: steps=57, seed=637907, balls=28, size=0.854059, restitution=0, friction=0.5, density=1, gravity=15, speed=30, dt=0.0489361, viter=1, piter=6, sleep=0, shape=1, joints=0, bullet=1, ccd=1, mirror=0, shift=0
- breaks learned invariant: kinetic stays within 31.5766 x (|its start| + 189236) of its own start (mined, relative): steps=197, seed=330294, balls=15, size=1.01284, restitution=0.5, friction=0.5, density=9.46086, gravity=22.5536, speed=20.2354, dt=0.0477534, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0, mirror=0, shift=0
- breaks learned invariant: max_abs_x stays within 3.73355 x (|its start| + 0.779318) of its own start (mined, relative): steps=49, seed=679354, balls=15, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=0, speed=60, dt=0.0395189, viter=1, piter=3, sleep=0, shape=0, joints=1, bullet=0, ccd=0, mirror=0, shift=0

Why, per finding:

- breaks relation: mirror=0|mirror=1 agree on max_abs_x,kinetic,max_speed,min_clearance over the first 2 steps (within 0.2 of the larger value) [min_clearance]: min_clearance is 1.85283 in one run and 0.890751 in the other at step 1. It depends on: balls = 28 (it disappears at 1, 15, 30); size = 0.854059 (it disappears at 0.1, 0.65, 1.2); restitution = 0 (it disappears at 0.5, 1); gravity = 0 or 15 (it disappears at 30); speed = 30 (it disappears at 0, 60); dt = 0.0489361 (it disappears at 0.004, 0.027, 0.05); viter = 1 (it disappears at 6, 12); piter = 1 or 6 (it disappears at 3); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1); bullet = 1 (it disappears at 0); ccd = 1 (it disappears at 0).
- breaks learned invariant: kinetic stays within 31.5766 x (|its start| + 189236) of its own start (mined, relative): kinetic stays within 31.5766 x (|its start| + 189236) of its own start (mined, relative) (observed 9941680.447967 vs limit 121578.604377 at step 167). It depends on: balls = 15 (it disappears at 1, 30); size = 1.2 or 1.01284 (it disappears at 0.1, 0.65); restitution = 0.5 or 1 (it disappears at 0); density = 10 or 9.46086 (it disappears at 0.1, 1); gravity = 30 or 22.5536 (it disappears at 0, 15); speed = 0 or 20.2354 (it disappears at 30, 60); dt = 0.05 or 0.0477534 (it disappears at 0.004, 0.027); joints = 0 (it disappears at 1).
- breaks learned invariant: max_abs_x stays within 3.73355 x (|its start| + 0.779318) of its own start (mined, relative): max_abs_x stays within 3.73355 x (|its start| + 0.779318) of its own start (mined, relative) (observed 38.349899 vs limit 7.395359 at step 48). It depends on: balls = 15 or 30 (it disappears at 1); size = 0.1 (it disappears at 0.65, 1.2); restitution = 0.5 or 1 (it disappears at 0); gravity = 0 (it disappears at 15, 30); speed = 60 (it disappears at 0, 30); dt = 0.0395189 (it disappears at 0.004, 0.027, 0.05); viter = 1 (it disappears at 6, 12); piter = 3 or 6 (it disappears at 1); shape = 0 (it disappears at 1, 2); joints = 1 (it disappears at 0); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=scans/N09/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
