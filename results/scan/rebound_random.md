# Scan of rebound-random

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 9 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| learned guess | breaks learned invariant: when integrator = bs, max |max_radius| follows spread^0.895676 within a factor 3 (mined, scaled) | 1 of 600 | steps=44, bodies=6, mass_ratio=97.8328, spread=3.0952, speed=0.665236, softening=0.209828, dt=0.0368478, integrator=bs |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: when integrator = bs, max |max_radius| follows spread^0.895676 within a factor 3 (mined, scaled): steps=44, bodies=6, mass_ratio=97.8328, spread=3.0952, speed=0.665236, softening=0.209828, dt=0.0368478, integrator=bs, layout=random

Why, per finding:

- breaks learned invariant: when integrator = bs, max |max_radius| follows spread^0.895676 within a factor 3 (mined, scaled): when integrator = bs, max |max_radius| follows spread^0.895676 within a factor 3 (mined, scaled) (observed 8.91983 vs about 1.79267 expected from spread=3.0952). It depends on: bodies = 6 (it disappears at 2, 4); mass_ratio = 100 or 97.8328 (it disappears at 1, 10); spread = 2 or 3.0952 (it disappears at 11, 20); speed = 0.665236 (it disappears at 0, 1, 2); softening = 0 or 0.209828 (it disappears at 0.5, 1); dt = 0.05 or 0.0368478 (it disappears at 0.001, 0.00707107); integrator = bs (it disappears at ias15, whfast, leapfrog).

Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
