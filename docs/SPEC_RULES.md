# Spec to rules: can a model write the tests from a description?

`rc_spec` gives the model only the plain-language description of a simulator (the same text the tester agent gets, with the
list of declared rules removed) plus the names of the per-step quantities the engine records. The model proposes invariants
in a small language (bound, conserved, monotone, exclusive; `src/spec/spec.hpp`). The engine, not the model, then:

1. runs every invariant on 1500 clean simulations and discards any that ever fails;
2. shows the model the first counterexample of each discarded one and allows **one** revision;
3. re-checks the survivors on 3000 fresh clean simulations (anything that still fails is dropped);
4. hunts: runs 300 random scenarios per seeded bug and counts the scenarios where a surviving invariant fires on the buggy
   simulator but **not** on the same scenario run clean (so a hit is attributable to the bug, not to the scenario).

For comparison it also counts, on the same scenarios, how often the hand-written rules (`rule_catalog`) fire.
The prompt contains no bug information and none of the hand-written rule ids (tested in `spec rules`).

Run: `./build/rc_spec --domain orbit --live --out results/spec_orbit` (about $0.015 per domain). Responses are cached in
`results/spec_*/cache.jsonl` and can be replayed with `--replay`. The table below is printed by
`./build/rc_spec summarize results/spec_rigid2d results/spec_orbit results/spec_cartpole results/spec_traffic`
(copy in `results/spec_summary.md`).

## Result (one run per domain, model `muse-spark-1.3`, total model cost $0.068)

Bugs hit at least once in the hunt: **model-written invariants 9 of 22, hand-written rules 19 of 22.**

| domain | invariants kept | bugs hit by model-written invariants | bugs hit by hand-written rules |
|---|---|---|---|
| orbit | 12 | o02, o03 (conservation of momentum and angular momentum); not o01 | o01, o02, o03 |
| cart-pole | 12 | c01, c03; not c02 | c01, c03; not c02 |
| traffic | 12 | none | t01, t02, t03 |
| rigid bodies | 10 | b03, b05, b08, b09, b13 (b03, b09 and b13 on 1 to 2 of 300 scenarios) | 11 of 13 (all except b06, b12) |

Per-bug counts are in `results/spec_summary.md`.

## What this shows, and what it does not

- Where a conservation law applies, the model finds it from the description alone: for orbits it proposed conservation of
  momentum and angular momentum, which fired on o02 (28 of 300 scenarios, against 26 for the hand-written rules) and o03
  (22 and 22). It never proposed energy conservation, so it missed o01, which is an energy drift.
- Many kept invariants are vacuous (for example `queue >= 0`, `speed <= 5000`). They cannot fail, so they never found
  anything. "Kept" means "never failed on clean runs", not "useful"; the useful number is the per-bug column.
- The rule language is the limit for the traffic domain: the failures there are about durations (a yellow or an all-red phase
  that is too short), which bound/conserved/monotone/exclusive cannot express. The model proposed the right safety property
  (north-south and east-west never active together) and it holds even with the bugs. Adding a duration form to the language
  is the obvious extension; it was not tuned after seeing these results.
- The cart-pole hits come from bounds that coincide with the declared force limit stated in the description; this is
  restating the spec, which is the point of the feature, not discovering something new.
- One run per domain, 300 scenarios per bug, no repeated model draws, so there are no confidence intervals. The hand-written
  rules were written by the author, who knew the simulators; they are an upper reference, not an independent baseline.
- The revision round was used once (rigid bodies, 1 invariant rejected on clean runs because the "gap" signal is a
  placeholder value when there are fewer than two bodies; the model's revision was kept).
- A first attempt returned nothing because the model spent the whole output budget on reasoning; the fix was a low reasoning
  effort and a larger output cap (a harness change, not a change to the prompt's content).
