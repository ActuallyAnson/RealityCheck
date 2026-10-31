# Scan of box2d-continuous-allowed-off

400 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 12 candidate invariants.

| finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|
| breaks learned invariant: kinetic <= 118238 (mined) | 2 of 400 | steps=200, radius=0.272223, gravity=30, dt=0.05, substeps=1 |
| breaks learned invariant: max_speed <= 261.632 (mined) | 2 of 400 | steps=200, gravity=30, speed=51.3902, dt=0.05 |
| breaks learned invariant: min_clearance >= -1567.84 (mined) | 1 of 400 | steps=200, gravity=30, speed=51.3902, dt=0.05 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: kinetic <= 118238 (mined): steps=200, balls=15, radius=0.272223, restitution=0.5, friction=0.5, gravity=30, speed=30, dt=0.05, substeps=1, continuous=0
- breaks learned invariant: max_speed <= 261.632 (mined): steps=200, balls=15, radius=0.8, restitution=0.5, friction=0.5, gravity=30, speed=51.3902, dt=0.05, substeps=4, continuous=0
- breaks learned invariant: min_clearance >= -1567.84 (mined): steps=200, balls=15, radius=0.8, restitution=0.5, friction=0.5, gravity=30, speed=51.3902, dt=0.05, substeps=4, continuous=0

Why, per finding:

- breaks learned invariant: kinetic <= 118238 (mined): kinetic <= 118238 (mined) (observed 119082.181449 vs limit 118238.399714 at step 186). It depends on: balls = 15 (it disappears at 1, 30); radius = 0.1 or 0.272223 (it disappears at 0.8, 1.5); restitution = 0.5 or 1 (it disappears at 0); friction = 0 or 0.5 (it disappears at 1); gravity = 30 (it disappears at 0, 15); speed = 30 or 60 (it disappears at 0); dt = 0.05 (it disappears at 0.004, 0.027); substeps = 1 (it disappears at 4, 8); continuous = 0 (it disappears at 1).
- breaks learned invariant: max_speed <= 261.632 (mined): max_speed <= 261.632 (mined) (observed 261.908905 vs limit 261.631943 at step 144). It depends on: balls = 15 (it disappears at 1, 30); radius = 0.1 or 0.8 (it disappears at 1.5); gravity = 30 (it disappears at 0, 15); speed = 60 or 51.3902 (it disappears at 0, 30); dt = 0.05 (it disappears at 0.004, 0.027); continuous = 0 (it disappears at 1).
- breaks learned invariant: min_clearance >= -1567.84 (mined): min_clearance >= -1567.84 (mined) (observed -1575.123120 vs limit -1567.843483 at step 178). It depends on: balls = 15 (it disappears at 1, 30); radius = 0.1 or 0.8 (it disappears at 1.5); gravity = 30 (it disappears at 0, 15); speed = 60 or 51.3902 (it disappears at 0, 30); dt = 0.05 (it disappears at 0.004, 0.027); continuous = 0 (it disappears at 1).

Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
