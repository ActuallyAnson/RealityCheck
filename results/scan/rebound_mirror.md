# Scan of rebound-mirror

600 scenarios (half pushed to parameter boundaries), 188 normal runs mined, 3 candidate invariants.

| finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|
| non-finite signal energy | 28 of 600 | steps=24, bodies=3, integrator=whfast, layout=mirror_pairs |
| run failed | 18 of 600 | steps=25, bodies=3, layout=mirror_pairs |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- non-finite signal energy: steps=24, bodies=3, mass_ratio=10, spread=11, speed=1, softening=0.5, dt=0.00707107, integrator=whfast, layout=mirror_pairs
- run failed: steps=25, bodies=3, mass_ratio=10, spread=11, speed=1, softening=0.5, dt=0.00707107, integrator=ias15, layout=mirror_pairs

Why, per finding:

- non-finite signal energy: signal energy is not finite at step 1. It depends on: bodies = 3 (it disappears at 2, 4, 6); integrator = whfast (it disappears at ias15, leapfrog, bs); layout = mirror_pairs (it disappears at random).
- run failed: the program failed or answered an error: external simulator did not answer within 5 s and was killed (a hang, not a finding). It depends on: bodies = 3 (it disappears at 2, 4, 6); integrator = ias15 (it disappears at whfast, leapfrog, bs); layout = mirror_pairs (it disappears at random).

Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
