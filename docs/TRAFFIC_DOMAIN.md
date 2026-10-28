# Fourth domain: a traffic-signal controller (discrete state machine, not physics)

Added to show the tester is not tied to rigid-body dynamics: a four-approach intersection with actuated signals,
emergency pre-emption, pedestrian requests and detector failures. One step is one second. Same tools, shrinker, agent and
checker interface; only the simulator, rule catalogue (`valid_signal`, `no_conflicting_signals`, `all_red_clearance`,
`yellow_duration`, `max_wait`), `inspect_state`/`render_frames` and the viewer renderer are new (`--domain traffic`).

Seeded bugs (hidden; descriptions in `data/ground_truth.json`, harness only): `t01`..`t03`, each active only under a
condition (an emergency pre-emption during a long yellow; a shortened yellow after an early gap-out with a long max green;
a detector failure that disables the max-green and recall logic).

Calibration (`rc_orbit_cal --domain traffic`): clean simulator 0 of 40000 random and 0 of 30000 extreme-biased scenarios
flagged.

Evaluation (`data/eval_lock_traffic.json`: seeds 5101..5505, controls 5901..5903, all three bugs fresh/held-out; unchanged
agent v1):

| bug | agent: valid and attributable | random baseline: attributable | random: median scenarios to first |
|---|---|---|---|
| t01 | 5/5 | 5/5 | 4 |
| t02 | 1/5 | 5/5 | 14 |
| t03 | 3/5 | 5/5 | 47 |

Controls: 0 reports in 3 runs. Cost $1.37 for 18 runs. Tables: `results/traffic_agent/summary.md`, `results/traffic/baseline.md`.

**Read this as a negative result for the agent.** Random testing finds every traffic bug in every seed; the agent is worse on
t02 and t03. Of the 9 failed agent runs, all 9 ended on the 26-turn cap (`stop_reason: turns`) without an accepted report,
so the shortfall is search efficiency and turn budget, not wrong reports: no invalid report was accepted and the controls
stayed clean. The bugs are not needles for random testing (needle size roughly 1 percent), so there is nothing for the
agent's hypotheses to beat. I did not rerun with a larger turn budget; doing so after seeing these numbers would be a second
attempt and would be reported as one.

Disclosures: the viewer draws each approach as a signal lamp plus a queue of cars; the lamp colour and queue length come from
the recorded states, nothing is simulated in the browser.
