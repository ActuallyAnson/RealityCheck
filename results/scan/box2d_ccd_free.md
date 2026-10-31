# Scan of box2d-continuous-allowed-off

400 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 10 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| learned guess | breaks learned invariant: kinetic stays within 5.52714 x (|its start| + 3941.28) of its own start (mined, relative) | 2 of 400 | steps=200, radius=0.272223, gravity=30, dt=0.05, substeps=1 |
| learned guess | breaks learned invariant: min_clearance >= -1567.84 (mined) | 1 of 400 | steps=200, gravity=30, speed=51.3902, dt=0.05 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: kinetic stays within 5.52714 x (|its start| + 3941.28) of its own start (mined, relative): steps=200, balls=15, radius=0.272223, restitution=0.5, friction=0.5, gravity=30, speed=30, dt=0.05, substeps=1, continuous=0
- breaks learned invariant: min_clearance >= -1567.84 (mined): steps=200, balls=15, radius=0.8, restitution=0.5, friction=0.5, gravity=30, speed=51.3902, dt=0.05, substeps=4, continuous=0

Why, per finding:

- breaks learned invariant: kinetic stays within 5.52714 x (|its start| + 3941.28) of its own start (mined, relative): kinetic stays within 5.52714 x (|its start| + 3941.28) of its own start (mined, relative) (observed 66161.271434 vs limit 6749.999908 at step 138). It depends on: balls = 15 (it disappears at 1, 30); radius = 0.1 or 0.272223 (it disappears at 0.8, 1.5); restitution = 0.5 or 1 (it disappears at 0); friction = 0 or 0.5 (it disappears at 1); gravity = 30 (it disappears at 0, 15); speed = 30 or 60 (it disappears at 0); dt = 0.05 (it disappears at 0.004, 0.027); substeps = 1 (it disappears at 4, 8); continuous = 0 (it disappears at 1).
- breaks learned invariant: min_clearance >= -1567.84 (mined): min_clearance >= -1567.84 (mined) (observed -1575.123120 vs limit -1567.843483 at step 178). It depends on: balls = 15 (it disappears at 1, 30); radius = 0.1 or 0.8 (it disappears at 1.5); gravity = 30 (it disappears at 0, 15); speed = 60 or 51.3902 (it disappears at 0, 30); dt = 0.05 (it disappears at 0.004, 0.027); continuous = 0 (it disappears at 1).

Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
