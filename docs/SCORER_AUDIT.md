# Audit of the keyword explanation scorer

`explanation_names_cause` (data/ground_truth.json, `cause_keywords`) passes an explanation when it contains at least one
keyword from every keyword group. It was fixed before the first agent run. This audit checks it by hand.

Method: 12 valid reports drawn at random (seed 7) from the saved agent runs (rigid-body v1, orbit, cart-pole attempt 1);
for each, the explanation was compared with the one-sentence ground truth and judged "names the true cause" or not.
The judge is the author of this repo (and the same assistant that wrote the scorer), not blind and not independent, so
treat the numbers as a rough check, not a measurement.

| run | scorer | hand judgment | note |
|---|---|---|---|
| o01_2101 | yes | yes | names the substep limit |
| b07_101 | yes | yes (borderline) | "wrong effective mass" with large mass ratio |
| b04_303 | no | no | describes tunneling, not shallow-corner skipping |
| c02_3505 (envelope 1) | no | partly | says the clamp is not applied; misses the push trigger |
| b06_101 | yes | yes | energy injected at stacked contacts |
| b02_202 | yes | no | hedges between contact impulses and "semi-implicit Euler"; never says the update order |
| b07_505 | yes | yes | mass ratio impulse error |
| b01_505 | yes | yes | thin wall, high speed, missing continuous handling |
| b03_404 | yes | no | mass-ratio ping-pong; nothing about tiny time steps |
| b10_404 | no | no | does not mention the large-radius bound |
| o01_2404 | yes | yes | names the 6-substep cap |
| o03_2303 | yes | no | blames substepping; "soften" matched by accident |

Agreement 8 of 12. Of the 8 the scorer passed, 5 were judged correct (precision about 0.6); one scorer "no" was a partial
match. So the keyword score overstates how often the cause is actually named, and "explanation names cause" in the
v1/orbit/cart-pole tables should be read as an upper bound.

Consequences:
- The scorer was not changed after seeing this (that would be tuning on results); the audit is reported next to it.
- For agent v2, the headline cause metric is **counterfactual localization**: the agent must submit a passing variant of
  the shrunk scenario that differs by at most two fields, and the changed fields are matched against `trigger_fields`
  in the ground truth. It needs no keyword matching. It is necessary, not sufficient (a different change can also remove
  a failure by luck), and the trigger lists were written by the author from the bug descriptions.
- `inspect_state` exposes `substeps_in_step`, which is why some explanations (o01, b11-style bugs) cite the substep
  count: part of "names cause" for those bugs is reading telemetry that the tool returns.
