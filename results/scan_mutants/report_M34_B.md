# Scan of M34_B

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 15 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| your rule | breaks learned invariant: required by you: n_outside <= 0 | 152 of 600 | steps=65, balls=29, size=0.985189, restitution=0.644752, friction=0.967598, piter=6 |
| learned guess | breaks learned invariant: n_outside <= 13.5 (mined) | 3 of 600 | steps=24, balls=30, size=0.1, speed=60, dt=0.0395189, viter=1 |
| learned guess | breaks learned invariant: kinetic stays within 5.80347 x (|its start| + 58117) of its own start (mined, relative) | 1 of 600 | steps=32, size=1.17091, restitution=1, dt=0.00433784, viter=12 |
| learned guess | breaks learned invariant: max_speed <= 379.672 (mined) | 1 of 600 | steps=32, size=1.17091, dt=0.00433784, viter=12 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: required by you: n_outside <= 0: steps=65, seed=26456, balls=29, size=0.985189, restitution=0.644752, friction=0.967598, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=6, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: n_outside <= 13.5 (mined): steps=24, seed=679354, balls=30, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=15, speed=60, dt=0.0395189, viter=1, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: kinetic stays within 5.80347 x (|its start| + 58117) of its own start (mined, relative): steps=32, seed=485944, balls=15, size=1.17091, restitution=1, friction=0.5, density=1, gravity=15, speed=30, dt=0.00433784, viter=12, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: max_speed <= 379.672 (mined): steps=32, seed=485944, balls=15, size=1.17091, restitution=0.5, friction=0.5, density=1, gravity=15, speed=30, dt=0.00433784, viter=12, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0

Why, per finding:

- breaks learned invariant: required by you: n_outside <= 0: required by you: n_outside <= 0 (observed 1.000000 vs limit 0.000000 at step 10). It depends on: balls = 30 or 29 (it disappears at 1, 15); restitution = 0.5 or 1 or 0.644752 (it disappears at 0); friction = 0 or 0.5 or 0.967598 (it disappears at 1); speed = 30 or 60 (it disappears at 0); dt = 0.027 or 0.05 (it disappears at 0.004); viter = 6 or 12 (it disappears at 1); piter = 6 (it disappears at 1, 3); shape = 1 or 2 (it disappears at 0); ccd = 0 (it disappears at 1).
- breaks learned invariant: n_outside <= 13.5 (mined): n_outside <= 13.5 (mined) (observed 14.000000 vs limit 13.500000 at step 7). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 (it disappears at 0.65, 1.2); speed = 60 (it disappears at 0, 30); dt = 0.05 or 0.0395189 (it disappears at 0.004, 0.027); ccd = 0 (it disappears at 1).
- breaks learned invariant: kinetic stays within 5.80347 x (|its start| + 58117) of its own start (mined, relative): kinetic stays within 5.80347 x (|its start| + 58117) of its own start (mined, relative) (observed 899164.930192 vs limit 39785.592825 at step 1). It depends on: balls = 15 or 30 (it disappears at 1); size = 1.2 or 1.17091 (it disappears at 0.1, 0.65); restitution = 0.5 or 1 (it disappears at 0); speed = 30 or 60 (it disappears at 0); dt = 0.004 or 0.00433784 (it disappears at 0.027, 0.05); viter = 6 or 12 (it disappears at 1); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1).
- breaks learned invariant: max_speed <= 379.672 (mined): max_speed <= 379.672 (mined) (observed 461.059052 vs limit 379.671936 at step 1). It depends on: balls = 15 or 30 (it disappears at 1); size = 1.2 or 1.17091 (it disappears at 0.1, 0.65); restitution = 0.5 or 1 (it disappears at 0); friction = 0.5 or 1 (it disappears at 0); speed = 30 or 60 (it disappears at 0); dt = 0.004 or 0.00433784 (it disappears at 0.027, 0.05); viter = 12 (it disappears at 1, 6); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M34_B/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
