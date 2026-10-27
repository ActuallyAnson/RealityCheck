# Testing an outside engine (Box2D 3.x)

`rc_box2d` runs random scenarios (no model, no API) through an **unmodified Box2D** world and checks the same
declared rules. The adapter is `src/external/box2d_sim.cpp`; walls get restitution 0 so bodies decide the bounce.
Controller scenarios are off (Box2D has no controller), so `controller_force_limit` cannot fire.
Reproduce: `./build/rc_box2d --scenarios 3000 [--no-continuous] --out results/box2d_*.json`.

| Configuration | in_arena | no_wall_penetration | no_tunneling | energy_bound |
|---|---|---|---|---|
| Box2D default (continuous on), 3000 scenarios | 889 | 199 | 0 | 0 |
| continuous collision off, 3000 scenarios | 2088 | 851 | 186 | 0 |

How to read it (these are not claims of new engine bugs):
- **Penetration flags with default settings** come from Box2D's soft contacts: a fast body sinks past our 0.15
  tolerance before being pushed out. That is a documented design trade-off, so it shows our declared tolerance,
  not an engine defect. The rule thresholds are ours.
- **Tunneling with continuous collision off** matches the known class of reports (e.g. forum threads about
  tunneling at low frame rates) and is the documented reason continuous collision exists. It is a
  misconfiguration check, not a newly found bug.
- No energy gain was flagged in either configuration.
