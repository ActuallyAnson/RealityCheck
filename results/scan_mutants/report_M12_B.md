# Scan of M12_B

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 15 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| your rule | breaks learned invariant: required by you: n_outside <= 0 | 152 of 600 | steps=66, size=0.352064 |
| learned guess | breaks learned invariant: n_outside <= 10.5 (mined) | 3 of 600 | steps=24, balls=30, size=0.1, speed=60, dt=0.0395189 |
| learned guess | breaks learned invariant: kinetic stays within 8.5453 x (|its start| + 58117) of its own start (mined, relative) | 2 of 600 | steps=65, balls=30, size=1.17091, restitution=1, dt=0.00433784 |
| learned guess | breaks learned invariant: kinetic changes by at most 2.71357e+06 per step (mined) | 1 of 600 | steps=97, balls=22, size=1.10008, friction=0.0365771, density=0.843613, gravity=30, dt=0.004, viter=12, joints=1 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: required by you: n_outside <= 0: steps=66, seed=496328, balls=15, size=0.352064, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: n_outside <= 10.5 (mined): steps=24, seed=679354, balls=30, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=15, speed=60, dt=0.0395189, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: kinetic stays within 8.5453 x (|its start| + 58117) of its own start (mined, relative): steps=65, seed=485944, balls=30, size=1.17091, restitution=1, friction=0.5, density=1, gravity=15, speed=30, dt=0.00433784, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: kinetic changes by at most 2.71357e+06 per step (mined): steps=97, seed=154361, balls=22, size=1.10008, restitution=0.5, friction=0.0365771, density=0.843613, gravity=30, speed=30, dt=0.004, viter=12, piter=3, sleep=0, shape=1, joints=1, bullet=0, ccd=0

Why, per finding:

- breaks learned invariant: required by you: n_outside <= 0: required by you: n_outside <= 0 (observed 1.000000 vs limit 0.000000 at step 10). It depends on: balls = 15 or 30 (it disappears at 1); size = 0.1 or 0.65 or 0.352064 (it disappears at 1.2); dt = 0.027 or 0.05 (it disappears at 0.004); ccd = 0 (it disappears at 1).
- breaks learned invariant: n_outside <= 10.5 (mined): n_outside <= 10.5 (mined) (observed 13.000000 vs limit 10.500000 at step 7). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 (it disappears at 0.65, 1.2); speed = 60 (it disappears at 0, 30); dt = 0.027 or 0.05 or 0.0395189 (it disappears at 0.004); ccd = 0 (it disappears at 1).
- breaks learned invariant: kinetic stays within 8.5453 x (|its start| + 58117) of its own start (mined, relative): kinetic stays within 8.5453 x (|its start| + 58117) of its own start (mined, relative) (observed 1874835.399479 vs limit 77502.000765 at step 62). It depends on: balls = 30 (it disappears at 1, 15); size = 0.65 or 1.2 or 1.17091 (it disappears at 0.1); restitution = 1 (it disappears at 0, 0.5); density = 1 or 10 (it disappears at 0.1); gravity = 0 or 15 (it disappears at 30); speed = 30 or 60 (it disappears at 0); dt = 0.00433784 (it disappears at 0.004, 0.027, 0.05); viter = 6 or 12 (it disappears at 1); piter = 1 or 3 (it disappears at 6); shape = 1 (it disappears at 0, 2).
- breaks learned invariant: kinetic changes by at most 2.71357e+06 per step (mined): kinetic changes by at most 2.71357e+06 per step (mined) (observed 3798301.541529 vs limit 2713572.575222 at step 11). It depends on: balls = 30 or 22 (it disappears at 1, 15); size = 1.10008 (it disappears at 0.1, 0.65, 1.2); restitution = 0.5 (it disappears at 0, 1); friction = 0.0365771 (it disappears at 0, 0.5, 1); density = 1 or 10 or 0.843613 (it disappears at 0.1); speed = 30 (it disappears at 0, 60); dt = 0.004 (it disappears at 0.027, 0.05); viter = 12 (it disappears at 1, 6); piter = 3 or 6 (it disappears at 1); shape = 1 (it disappears at 0, 2); joints = 1 (it disappears at 0); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M12_B/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
