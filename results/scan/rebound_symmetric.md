# Scan of rebound-symmetric

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 9 candidate invariants.

| finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|
| breaks learned invariant: energy <= 133646 (mined) | 4 of 600 | steps=20, spread=2, speed=0, softening=0, dt=0.0116902, integrator=whfast, layout=equal_masses_zero_momentum |
| breaks learned invariant: energy >= -57593.3 (mined) | 1 of 600 | steps=22, bodies=6, mass_ratio=97.8328, spread=3.0952 |
| breaks learned invariant: energy changes by at most 235978 per step (mined) | 4 of 600 | steps=20, spread=2, speed=0, softening=0, dt=0.0116902, integrator=whfast, layout=equal_masses_zero_momentum |
| breaks learned invariant: max_radius <= 130.092 (mined) | 2 of 600 | steps=37, spread=2, speed=0, softening=0, dt=0.0116902, integrator=whfast, layout=equal_masses_zero_momentum |
| breaks learned invariant: max_radius changes by at most 14.9789 per step (mined) | 1 of 600 | steps=20, bodies=5, spread=4.41247, integrator=whfast, layout=equal_masses_zero_momentum |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: energy <= 133646 (mined): steps=20, bodies=4, mass_ratio=10, spread=2, speed=0, softening=0, dt=0.0116902, integrator=whfast, layout=equal_masses_zero_momentum
- breaks learned invariant: energy >= -57593.3 (mined): steps=22, bodies=6, mass_ratio=97.8328, spread=3.0952, speed=1, softening=0.5, dt=0.00707107, integrator=ias15, layout=random
- breaks learned invariant: energy changes by at most 235978 per step (mined): steps=20, bodies=4, mass_ratio=10, spread=2, speed=0, softening=0, dt=0.0116902, integrator=whfast, layout=equal_masses_zero_momentum
- breaks learned invariant: max_radius <= 130.092 (mined): steps=37, bodies=4, mass_ratio=10, spread=2, speed=0, softening=0, dt=0.0116902, integrator=whfast, layout=equal_masses_zero_momentum
- breaks learned invariant: max_radius changes by at most 14.9789 per step (mined): steps=20, bodies=5, mass_ratio=10, spread=4.41247, speed=1, softening=0.5, dt=0.00707107, integrator=whfast, layout=equal_masses_zero_momentum

Why, per finding:

- breaks learned invariant: energy <= 133646 (mined): energy <= 133646 (mined) (observed 778320.264014 vs limit 133645.922976 at step 19). It depends on: bodies = 4 or 6 (it disappears at 2); spread = 2 (it disappears at 11, 20); speed = 0 (it disappears at 1, 2); softening = 0 (it disappears at 0.5, 1); dt = 0.0116902 (it disappears at 0.001, 0.00707107, 0.05); integrator = whfast or leapfrog (it disappears at ias15, bs); layout = equal_masses_zero_momentum (it disappears at random).
- breaks learned invariant: energy >= -57593.3 (mined): energy >= -57593.3 (mined) (observed -59964.950123 vs limit -57593.317642 at step 0). It depends on: bodies = 6 (it disappears at 2, 4); mass_ratio = 100 or 97.8328 (it disappears at 1, 10); spread = 2 or 3.0952 (it disappears at 11, 20); softening = 0 or 0.5 (it disappears at 1); layout = random (it disappears at equal_masses_zero_momentum).
- breaks learned invariant: energy changes by at most 235978 per step (mined): energy changes by at most 235978 per step (mined) (observed 778332.002132 vs limit 235978.055718 at step 19). It depends on: bodies = 4 or 6 (it disappears at 2); spread = 2 (it disappears at 11, 20); speed = 0 (it disappears at 1, 2); softening = 0 (it disappears at 0.5, 1); dt = 0.0116902 (it disappears at 0.001, 0.00707107, 0.05); integrator = whfast or leapfrog (it disappears at ias15, bs); layout = equal_masses_zero_momentum (it disappears at random).
- breaks learned invariant: max_radius <= 130.092 (mined): max_radius <= 130.092 (mined) (observed 139.828494 vs limit 130.091550 at step 32). It depends on: bodies = 4 or 6 (it disappears at 2); spread = 2 (it disappears at 11, 20); speed = 0 (it disappears at 1, 2); softening = 0 (it disappears at 0.5, 1); dt = 0.05 or 0.0116902 (it disappears at 0.001, 0.00707107); integrator = whfast or leapfrog (it disappears at ias15, bs); layout = equal_masses_zero_momentum (it disappears at random).
- breaks learned invariant: max_radius changes by at most 14.9789 per step (mined): max_radius changes by at most 14.9789 per step (mined) (observed 765.585608 vs limit 14.978926 at step 1). It depends on: bodies = 6 or 5 (it disappears at 2, 4); spread = 2 or 4.41247 (it disappears at 11, 20); dt = 0.00707107 or 0.05 (it disappears at 0.001); integrator = whfast (it disappears at ias15, leapfrog, bs); layout = equal_masses_zero_momentum (it disappears at random).

Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
