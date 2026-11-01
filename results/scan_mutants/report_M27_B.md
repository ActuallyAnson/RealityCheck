# Scan of M27_B

600 scenarios (half pushed to parameter boundaries), 199 normal runs mined, 13 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| HARD FAILURE | non-finite signal kinetic | 190 of 600 | steps=65, shape=2, ccd=1 |
| HARD FAILURE | run failed | 6 of 600 | steps=174, balls=28, shape=0, bullet=1, ccd=1 |
| your rule | breaks learned invariant: required by you: n_outside <= 0 | 206 of 600 | steps=66, size=0.352064 |
| learned guess | breaks learned invariant: kinetic stays within 4.15822 x (|its start| + 58117) of its own start (mined, relative) | 190 of 600 | steps=65, shape=2, ccd=1 |
| learned guess | breaks learned invariant: max_abs_x <= 266.448 (mined) | 1 of 600 | steps=144, gravity=0, speed=60, dt=0.0473404, ccd=1 |
| learned guess | breaks learned invariant: max_speed stays within 9.96333 x (|its start| + 4.74674) of its own start (mined, relative) | 1 of 600 | steps=160, speed=0.0503681, ccd=1 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- non-finite signal kinetic: steps=65, seed=891753, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=2, joints=0, bullet=0, ccd=1
- run failed: steps=174, seed=153250, balls=28, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=0, joints=0, bullet=1, ccd=1
- breaks learned invariant: required by you: n_outside <= 0: steps=66, seed=496328, balls=15, size=0.352064, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: kinetic stays within 4.15822 x (|its start| + 58117) of its own start (mined, relative): steps=65, seed=891753, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=2, joints=0, bullet=0, ccd=1
- breaks learned invariant: max_abs_x <= 266.448 (mined): steps=144, seed=633937, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=0, speed=60, dt=0.0473404, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=1
- breaks learned invariant: max_speed stays within 9.96333 x (|its start| + 4.74674) of its own start (mined, relative): steps=160, seed=586721, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=0.0503681, dt=0.027, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=1

Why, per finding:

- non-finite signal kinetic: signal kinetic is not finite at step 5. It depends on: shape = 0 or 2 (it disappears at 1); ccd = 1 (it disappears at 0).
- run failed: the program failed or answered an error: external simulator did not answer within 10 s and was killed (a hang, not a finding). It depends on: balls = 30 or 28 (it disappears at 1, 15); shape = 0 or 2 (it disappears at 1); bullet = 1 (it disappears at 0); ccd = 1 (it disappears at 0).
- breaks learned invariant: required by you: n_outside <= 0: required by you: n_outside <= 0 (observed 1.000000 vs limit 0.000000 at step 10). It depends on: balls = 15 or 30 (it disappears at 1); size = 0.1 or 0.352064 (it disappears at 0.65, 1.2); dt = 0.027 or 0.05 (it disappears at 0.004).
- breaks learned invariant: kinetic stays within 4.15822 x (|its start| + 58117) of its own start (mined, relative): kinetic stays within 4.15822 x (|its start| + 58117) of its own start (mined, relative) (observed nan vs limit 0.000000 at step 5). It depends on: shape = 0 or 2 (it disappears at 1); ccd = 1 (it disappears at 0).
- breaks learned invariant: max_abs_x <= 266.448 (mined): max_abs_x <= 266.448 (mined) (observed 266.534973 vs limit 266.448189 at step 132). It depends on: balls = 15 or 30 (it disappears at 1); gravity = 0 (it disappears at 15, 30); speed = 60 (it disappears at 0, 30); dt = 0.05 or 0.0473404 (it disappears at 0.004, 0.027); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1); ccd = 1 (it disappears at 0).
- breaks learned invariant: max_speed stays within 9.96333 x (|its start| + 4.74674) of its own start (mined, relative): max_speed stays within 9.96333 x (|its start| + 4.74674) of its own start (mined, relative) (observed 48.241909 vs limit 0.050368 at step 119). It depends on: balls = 15 or 30 (it disappears at 1); gravity = 15 or 30 (it disappears at 0); speed = 0 or 0.0503681 (it disappears at 30, 60); dt = 0.027 (it disappears at 0.004, 0.05); shape = 1 (it disappears at 0, 2); ccd = 1 (it disappears at 0).

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M27_B/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
