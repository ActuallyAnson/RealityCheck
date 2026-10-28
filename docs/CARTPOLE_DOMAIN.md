# Third domain: cart-pole (control / robotics)

A pole balanced on a cart by a fixed state-feedback controller (pole placement for the scenario's plant, RK4 plant,
zero-order hold at the outer step). Scenario: plant masses and length, gravity, initial state, force limit, sensor
noise, up to three pushes. Rules: finite state, pole within 0.8 rad, cart within +/-2.4 m, controller force within the
limit, energy bounded by initial energy plus the work of all external forces. Seeded bugs (hidden): `c01`..`c03`.
Run with `rc_agent --domain cartpole` or `rc_eval ... --lock data/eval_lock_cartpole.json`.

## The honest history (three attempts)

The scenario envelope is the controller's specification: inside it, the clean system must keep every rule. Writing it
down carefully took three tries, and the agent found the holes each time.

1. **Envelope 1** (`data/eval_lock_cartpole_envelope1.json`, `results/*_envelope1`): random sampling found 0 clean
   failures in 40000 scenarios, but 3 of 3 agent control runs found violations on the clean simulator. The agent built
   corner cases (light cart, heavy short pole, strong gravity, low force limit, a push) that no controller can recover.
2. **Envelope 2** (`data/eval_lock_cartpole_envelope2.json`): narrowed. A sweep of the corners of the box (one push) found
   0 failures, so the run started. A later sweep with three stacked pushes found 248 of 16384 corners failing on the
   clean simulator (overlapping pushes add up), so the run was stopped after 13 of 18 runs
   (`results/cartpole_agent_envelope2_partial`).
3. **Envelope 3** (current lock): the total of overlapping pushes is capped at 4 N. Verified: 0 of 16384 corners
   (including three stacked pushes), 0 of 40000 random scenarios, and 0 of 3 agent controls.

Lesson recorded for every domain: random sampling is not enough to declare a simulator clean; check the corners.
Because the agent had seen c01..c03 in attempts 1 and 2, they are not fresh held-out bugs in attempt 3.

## Numbers

Generated tables: `docs/RESULTS.md`. Needle bug: `c02` fires only when the controller demand exceeds the force limit
while a push above 2.5 N is active; random search finds it in roughly 1 of 2000 scenarios.
