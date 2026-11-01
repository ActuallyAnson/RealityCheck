# Scan of M10_A

600 scenarios (half pushed to parameter boundaries), 194 normal runs mined, 12 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| HARD FAILURE | non-finite signal kinetic | 463 of 600 | steps=65 |
| HARD FAILURE | run failed | 10 of 600 | steps=174, balls=28, joints=1, bullet=1, ccd=1 |
| learned guess | breaks learned invariant: kinetic stays within 1.84891 x (|its start| + 58117) of its own start (mined, relative) | 463 of 600 | steps=65 |
| learned guess | breaks learned invariant: max_abs_x <= 170.007 (mined) | 5 of 600 | steps=178, speed=46.967, dt=0.0386227 |
| learned guess | breaks learned invariant: n_outside <= 22.5 (mined) | 1 of 600 | steps=148, balls=30, speed=0, dt=0.0119832, viter=7, piter=1 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- non-finite signal kinetic: steps=65, seed=26456, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- run failed: steps=174, seed=153250, balls=28, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=1, joints=1, bullet=1, ccd=1
- breaks learned invariant: kinetic stays within 1.84891 x (|its start| + 58117) of its own start (mined, relative): steps=65, seed=26456, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: max_abs_x <= 170.007 (mined): steps=178, seed=4016, balls=15, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=46.967, dt=0.0386227, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: n_outside <= 22.5 (mined): steps=148, seed=640666, balls=30, size=0.65, restitution=0.5, friction=0.5, density=1, gravity=15, speed=0, dt=0.0119832, viter=7, piter=1, sleep=0, shape=1, joints=0, bullet=0, ccd=0

Why, per finding:

- non-finite signal kinetic: signal kinetic is not finite at step 4. It depends on: balls = 15 (it disappears at 1, 30); size = 0.1 or 0.65 (it disappears at 1.2); speed = 30 or 60 (it disappears at 0); ccd = 0 (it disappears at 1).
- run failed: the program failed or answered an error: external simulator did not answer within 10 s and was killed (a hang, not a finding). It depends on: balls = 30 or 28 (it disappears at 1, 15); size = 0.65 or 1.2 (it disappears at 0.1); speed = 30 (it disappears at 0, 60); dt = 0.004 or 0.027 (it disappears at 0.05); piter = 3 or 6 (it disappears at 1); joints = 1 (it disappears at 0); bullet = 1 (it disappears at 0); ccd = 1 (it disappears at 0).
- breaks learned invariant: kinetic stays within 1.84891 x (|its start| + 58117) of its own start (mined, relative): kinetic stays within 1.84891 x (|its start| + 58117) of its own start (mined, relative) (observed nan vs limit 0.000000 at step 4). It depends on: balls = 15 (it disappears at 1, 30); size = 0.1 or 0.65 (it disappears at 1.2); speed = 30 or 60 (it disappears at 0); ccd = 0 (it disappears at 1).
- breaks learned invariant: max_abs_x <= 170.007 (mined): max_abs_x <= 170.007 (mined) (observed 170.483398 vs limit 170.006905 at step 140). It depends on: balls = 15 or 30 (it disappears at 1); gravity = 0 or 15 (it disappears at 30); speed = 60 or 46.967 (it disappears at 0, 30); dt = 0.05 or 0.0386227 (it disappears at 0.004, 0.027); shape = 1 (it disappears at 0, 2); ccd = 0 (it disappears at 1).
- breaks learned invariant: n_outside <= 22.5 (mined): n_outside <= 22.5 (mined) (observed 23.000000 vs limit 22.500000 at step 125). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 or 0.65 (it disappears at 1.2); restitution = 0.5 (it disappears at 0, 1); friction = 0.5 (it disappears at 0, 1); density = 1 or 10 (it disappears at 0.1); gravity = 15 or 30 (it disappears at 0); speed = 0 (it disappears at 30, 60); dt = 0.0119832 (it disappears at 0.004, 0.027, 0.05); viter = 7 (it disappears at 1, 6, 12); piter = 1 (it disappears at 3, 6); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M10_A/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
