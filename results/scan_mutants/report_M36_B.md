# Scan of M36_B

600 scenarios (half pushed to parameter boundaries), 198 normal runs mined, 13 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| HARD FAILURE | non-finite signal kinetic | 497 of 600 | steps=65 |
| HARD FAILURE | run failed | 9 of 600 | steps=174, balls=28, shape=0, bullet=1, ccd=1 |
| your rule | breaks learned invariant: required by you: n_outside <= 0 | 159 of 600 | steps=98 |
| learned guess | breaks learned invariant: kinetic stays within 14.8005 x (|its start| + 58117) of its own start (mined, relative) | 497 of 600 | steps=65 |
| learned guess | breaks learned invariant: n_outside <= 18 (mined) | 4 of 600 | steps=53, balls=30, size=0.1 |
| learned guess | breaks learned invariant: max_speed changes by at most 201.179 per step (mined) | 2 of 600 | steps=55, speed=59.6961, dt=0.0451358, viter=2 |
| learned guess | breaks learned invariant: max_speed stays within 18.0533 x (|its start| + 4.90788) of its own start (mined, relative) | 2 of 600 | steps=55, speed=59.6961, dt=0.0451358, viter=2 |
| learned guess | breaks learned invariant: max_abs_x <= 243.561 (mined) | 1 of 600 | steps=144, gravity=0, speed=60, dt=0.0473404 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- non-finite signal kinetic: steps=65, seed=26456, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- run failed: steps=174, seed=153250, balls=28, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=0, joints=0, bullet=1, ccd=1
- breaks learned invariant: required by you: n_outside <= 0: steps=98, seed=533504, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: kinetic stays within 14.8005 x (|its start| + 58117) of its own start (mined, relative): steps=65, seed=26456, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: n_outside <= 18 (mined): steps=53, seed=340534, balls=30, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: max_speed changes by at most 201.179 per step (mined): steps=55, seed=233993, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=59.6961, dt=0.0451358, viter=2, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: max_speed stays within 18.0533 x (|its start| + 4.90788) of its own start (mined, relative): steps=55, seed=233993, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=59.6961, dt=0.0451358, viter=2, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: max_abs_x <= 243.561 (mined): steps=144, seed=633937, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=0, speed=60, dt=0.0473404, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0

Why, per finding:

- non-finite signal kinetic: signal kinetic is not finite at step 4. It depends on: balls = 15 or 30 (it disappears at 1); size = 0.1 or 0.65 (it disappears at 1.2); speed = 30 or 60 (it disappears at 0).
- run failed: the program failed or answered an error: external simulator did not answer within 10 s and was killed (a hang, not a finding). It depends on: balls = 30 or 28 (it disappears at 1, 15); shape = 0 or 2 (it disappears at 1); bullet = 1 (it disappears at 0); ccd = 1 (it disappears at 0).
- breaks learned invariant: required by you: n_outside <= 0: required by you: n_outside <= 0 (observed 1.000000 vs limit 0.000000 at step 15). It depends on: balls = 15 or 30 (it disappears at 1); dt = 0.027 or 0.05 (it disappears at 0.004); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1).
- breaks learned invariant: kinetic stays within 14.8005 x (|its start| + 58117) of its own start (mined, relative): kinetic stays within 14.8005 x (|its start| + 58117) of its own start (mined, relative) (observed nan vs limit 0.000000 at step 4). It depends on: balls = 15 or 30 (it disappears at 1); size = 0.1 or 0.65 (it disappears at 1.2); speed = 30 or 60 (it disappears at 0).
- breaks learned invariant: n_outside <= 18 (mined): n_outside <= 18 (mined) (observed 19.000000 vs limit 18.000000 at step 23). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 (it disappears at 0.65, 1.2); dt = 0.027 or 0.05 (it disappears at 0.004); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1).
- breaks learned invariant: max_speed changes by at most 201.179 per step (mined): max_speed changes by at most 201.179 per step (mined) (observed nan vs limit 0.000000 at step 5). It depends on: balls = 15 (it disappears at 1, 30); size = 0.65 (it disappears at 0.1, 1.2); friction = 0.5 or 1 (it disappears at 0); speed = 60 or 59.6961 (it disappears at 0, 30); dt = 0.05 or 0.0451358 (it disappears at 0.004, 0.027); viter = 2 (it disappears at 1, 6, 12); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1).
- breaks learned invariant: max_speed stays within 18.0533 x (|its start| + 4.90788) of its own start (mined, relative): max_speed stays within 18.0533 x (|its start| + 4.90788) of its own start (mined, relative) (observed nan vs limit 0.000000 at step 5). It depends on: balls = 15 (it disappears at 1, 30); size = 0.65 (it disappears at 0.1, 1.2); friction = 0.5 or 1 (it disappears at 0); speed = 60 or 59.6961 (it disappears at 0, 30); dt = 0.05 or 0.0451358 (it disappears at 0.004, 0.027); viter = 2 (it disappears at 1, 6, 12); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1).
- breaks learned invariant: max_abs_x <= 243.561 (mined): max_abs_x <= 243.561 (mined) (observed 244.567688 vs limit 243.561310 at step 121). It depends on: balls = 15 or 30 (it disappears at 1); size = 0.1 or 0.65 (it disappears at 1.2); gravity = 0 (it disappears at 15, 30); speed = 60 (it disappears at 0, 30); dt = 0.05 or 0.0473404 (it disappears at 0.004, 0.027); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M36_B/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
