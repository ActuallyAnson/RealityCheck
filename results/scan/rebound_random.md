# Scan of rebound-random

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 10 candidate invariants.

| finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|
| breaks learned invariant: momentum <= 103.644 (mined) | 4 of 600 | steps=20, mass_ratio=100, speed=1.97125 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: momentum <= 103.644 (mined): steps=20, bodies=4, mass_ratio=100, spread=11, speed=1.97125, softening=0.5, dt=0.00707107, integrator=ias15, layout=random

Why, per finding:

- breaks learned invariant: momentum <= 103.644 (mined): momentum <= 103.644 (mined) (observed 115.547067 vs limit 103.643972 at step 0). It depends on: bodies = 4 or 6 (it disappears at 2); mass_ratio = 100 (it disappears at 1, 10); speed = 2 or 1.97125 (it disappears at 0, 1).

Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
