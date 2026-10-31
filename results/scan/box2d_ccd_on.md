# Scan of box2d-continuous-on

1000 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 8 candidate invariants.

| finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|
| breaks learned invariant: max_abs_x stays within 4.86269 x (|its start| + 0.496511) of its own start (mined, relative) | 11 of 1000 | steps=59, balls=1 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: max_abs_x stays within 4.86269 x (|its start| + 0.496511) of its own start (mined, relative): steps=59, balls=1, radius=0.775, restitution=0.5, friction=0.5, gravity=15, speed=30, dt=0.027, substeps=4, continuous=1

Why, per finding:

- breaks learned invariant: max_abs_x stays within 4.86269 x (|its start| + 0.496511) of its own start (mined, relative): max_abs_x stays within 4.86269 x (|its start| + 0.496511) of its own start (mined, relative) (observed 7.657743 vs limit 0.891680 at step 45). It depends on: balls = 1 (it disappears at 20, 40); restitution = 0 or 0.5 (it disappears at 1); speed = 30 or 60 (it disappears at 0); dt = 0.027 or 0.05 (it disappears at 0.004).

Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
