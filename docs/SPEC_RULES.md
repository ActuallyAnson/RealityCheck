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
  placeholder value when there are fewer than two bodies; the model's revision was a vacuous bound of 2e9, which was kept and cannot fail).
- A first attempt returned nothing because the model spent the whole output budget on reasoning; the fix was a low reasoning
  effort and a larger output cap (a harness change, not a change to the prompt's content).

## Second attempt on traffic: duration forms (language 2)

After the first result I added two forms to the language, `min_run` and `max_run` (every run of steps where a signal equals a value lasts at least or at most N
steps, optionally N read from a constant signal), and three constant signals for traffic (`yellow_seconds`, `all_red_seconds`, `max_green_seconds`) and `any_active`.
Language 1 is unchanged and its prompt is byte-identical. This is a second attempt on a domain where the first failed, made after seeing that failure; both are reported.

Result (`results/spec_traffic_lang2/`, one run, $0.017): **0 of 3 traffic bugs hit again.** The model did use the new forms, but with constants it could not
justify: `min_run(any_active == 0, steps 1)` (an all-red of at least one second) and `max_run(any_active == 0, steps 30)`, instead of comparing yellow and all-red runs
with the scenario's own `yellow_seconds` and `all_red_seconds`, which were available as signals. Those invariants hold on every clean run and also on the buggy ones.
It also spent three of twelve slots on the constants being constant. So the language was not the only limit: the model proposes invariants that are safe to keep (they must
survive clean validation) and therefore loose. I stopped here rather than keep rewording the prompt until it passed.

## A no-model comparison: invariants mined from the clean simulator (`rc_spec --mined`)

Same hunt, same 22 seeded bugs, 300 scenarios each, but the invariants come from `src/spec/miner.cpp` run on 300 clean runs of the trusted simulator instead of from a model (tolerance factors from `results/mutation/mine_factors.tsv`; every mined invariant is also checked on 3000 fresh clean runs; cost $0).
This is a different protocol from `docs/MINING.md`, where each mutant mined from itself: here the reference is the unmodified simulator, which is what a regression setting provides. Reproduce: `rc_spec --mined --domain D --mine-bound B --mine-drift F --mine-step S --out results/spec_mined_D` and `rc_spec compare ...` (output in `results/spec_mined_compare.md`).

Bugs hit at least once: **model-written 9 of 22, mined from the clean simulator 10 of 22, either 11 of 22, hand-written rules 19 of 22.**
The mined invariants find what the model missed where a numeric regularity exists (the orbit energy drift o01, the traffic bug t03 on 5 of 300 scenarios) and miss what it found by reading the description (b13, the force limit); the union adds only 1 or 2 bugs, so the gap to the hand-written rules (19) is not closed by either source or by both.
The remaining 8 bugs that the hand-written rules catch and neither source does are the ones that need domain knowledge of what the rule is (a wall that must not be penetrated, a minimum phase duration), which is the knowledge the author supplied.

