# Adding a domain

A domain is a simulator plus its declared rules, behind the same tools, shrinker, agent, viewer and API. The traffic-signal domain was added this way (one commit:
27 files, +853 lines, of which the simulator is 325), and is the worked example. What a new domain `foo` needs, in the order I would do it:

1. **Scenario schema** (`src/sim/foo.hpp/.cpp`): a struct of the parameters, `foo_to_json`, `foo_from_json`, `foo_validate` (public limits; every sampled scenario must be valid), and
   `random_foo_scenario(seed)` drawing from those limits. Reuse `Scenario` (add a member struct, as `TrafficDef` does) so tools, shrinker and API need no new type.
2. **Simulator** `simulate_foo(Scenario, BugMask) -> Trace`: a pure function of its inputs. Record one `StepRecord` per step; map your state onto `body[i] = {x, y, vx, vy}` (the traffic
   domain stores signal, queue, wait and a flag) plus `energy`, `work`, `fmag` if relevant. Seeded bugs, if you want them for evaluation, hide behind `BugMask` bits.
3. **Rules** (`src/rules/checker.cpp`): `rule_catalog("foo")` (id and one-line text, which also feeds the prompt) and `check_foo_rules(Scenario, Trace)` returning the earliest violation per rule.
   Optionally add continuous margins to `rule_margins`.
4. **Dispatch**: `scenario.cpp` (JSON, validate, equality, size), `world.cpp` (`simulate`), `domains.hpp` (`random_scenario_for`, bug count and names), `eval/ground_truth.hpp` (bug index).
5. **Tools and prompt** (`src/tools/tools.cpp`): `tool_specs("foo")` (scenario format text), `simulation_description("foo")` (plain-language behaviour, limits and the declared rules; it must not mention any
   seeded bug), and an `inspect_foo` for `inspect_state`. `src/tools/render.cpp` draws the frame strip. Keep every existing domain's prompt and schema bytes unchanged (the `rigid2d pins` test enforces it for rigid bodies).
6. **Shrinker** (`src/shrink/shrink.cpp`): domain-specific reduction moves (drop an event, shorten the run, move a value to a default); the shrinker only accepts a candidate that still fails the same rule.
7. **Viewer** (`src/viewer/view_export.cpp`, `web/app.js`): export entities and frames, and add a renderer for any new entity kind; nothing else in the viewer changes.
8. **Evaluation**: calibrate bug triggers and the clean simulator with `rc_orbit_cal --domain foo` (random scenarios and extreme-biased ones must give 0 clean violations), write a lock file
   in `data/` and ground truth with `trigger_fields` in `data/ground_truth.json` (harness only), then run `rc_eval baseline` before any agent run.
9. **Tests**: determinism, clean-simulator-has-no-violations, each rule fires on a hand-built failing trace, tools and prompt leak check, shrinker round trip. Then run the mutation benchmark on it.

What is still manual and easy to forget: the `--domain` flags of the command-line tools, the demo's `domain_for` bug-id prefixes, and `spec` signals (`src/spec/spec.cpp`) if you want spec-to-rules.
Honest cost: for the traffic domain the simulator and rules were the smaller part; calibration (a clean simulator that never trips a rule on any scenario the sampler can draw) took the most care, as the cart-pole history in `docs/CARTPOLE_DOMAIN.md` shows.
