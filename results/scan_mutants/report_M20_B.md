# Scan of M20_B

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 14 candidate invariants.

| type | finding | scenarios | smallest example (values left at their default are omitted) |
|---|---|---|---|
| your rule | breaks learned invariant: required by you: n_outside <= 0 | 144 of 600 | steps=66, restitution=0.253143, shape=0 |
| learned guess | breaks learned invariant: n_outside <= 12 (mined) | 3 of 600 | steps=24, balls=30, size=0.1, speed=60, dt=0.0395189, viter=1 |
| learned guess | breaks learned invariant: max_abs_x <= 203.623 (mined) | 2 of 600 | steps=143, balls=28, size=0.403771, gravity=13.037, speed=40.1778, dt=0.0397896 |

Full parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):

- breaks learned invariant: required by you: n_outside <= 0: steps=66, seed=496328, balls=15, size=0.65, restitution=0.253143, friction=0.5, density=1, gravity=15, speed=30, dt=0.027, viter=6, piter=3, sleep=0, shape=0, joints=0, bullet=0, ccd=0
- breaks learned invariant: n_outside <= 12 (mined): steps=24, seed=679354, balls=30, size=0.1, restitution=0.5, friction=0.5, density=1, gravity=15, speed=60, dt=0.0395189, viter=1, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0
- breaks learned invariant: max_abs_x <= 203.623 (mined): steps=143, seed=911597, balls=28, size=0.403771, restitution=0.5, friction=0.5, density=1, gravity=13.037, speed=40.1778, dt=0.0397896, viter=6, piter=3, sleep=0, shape=1, joints=0, bullet=0, ccd=0

Why, per finding:

- breaks learned invariant: required by you: n_outside <= 0: required by you: n_outside <= 0 (observed 1.000000 vs limit 0.000000 at step 12). It depends on: balls = 15 or 30 (it disappears at 1); size = 0.1 or 0.65 (it disappears at 1.2); restitution = 0.5 or 1 or 0.253143 (it disappears at 0); gravity = 15 or 30 (it disappears at 0); speed = 30 or 60 (it disappears at 0); dt = 0.027 or 0.05 (it disappears at 0.004); viter = 6 or 12 (it disappears at 1); shape = 0 (it disappears at 1, 2); joints = 0 (it disappears at 1); ccd = 0 (it disappears at 1).
- breaks learned invariant: n_outside <= 12 (mined): n_outside <= 12 (mined) (observed 14.000000 vs limit 12.000000 at step 7). It depends on: balls = 30 (it disappears at 1, 15); size = 0.1 (it disappears at 0.65, 1.2); speed = 60 (it disappears at 0, 30); dt = 0.05 or 0.0395189 (it disappears at 0.004, 0.027); ccd = 0 (it disappears at 1).
- breaks learned invariant: max_abs_x <= 203.623 (mined): max_abs_x <= 203.623 (mined) (observed 203.945618 vs limit 203.622883 at step 134). It depends on: balls = 30 or 28 (it disappears at 1, 15); size = 0.1 or 0.403771 (it disappears at 0.65, 1.2); gravity = 0 or 13.037 (it disappears at 15, 30); speed = 60 or 40.1778 (it disappears at 0, 30); dt = 0.0397896 (it disappears at 0.004, 0.027, 0.05); shape = 1 (it disappears at 0, 2); joints = 0 (it disappears at 1); ccd = 0 (it disappears at 1).

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M20_B/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
