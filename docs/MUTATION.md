# Mutation benchmark: bugs sampled by a program, not planted by me

Every seeded bug elsewhere in this repository was written by me, which is a fair criticism of any result on them. This benchmark replaces my choice with a
mechanical one. `rc_mutgen` lists every mutation site in a simulator's step function and helpers (comparison operators `< <= > >= == !=`, `&& ||`, `+ - * /`,
`+= -=`, and numeric constants), draws 40 per domain uniformly with a fixed seed (1), and writes one mutated copy of the source per site. Each mutant is compiled
with the project's flags and run through `mut_eval` on the same pool of 1000 random scenarios as the unmodified simulator. Locked driver: `scripts/run_mutation_all.sh`
(parameters fixed before the run); per-mutant tables: `results/mutation/<domain>/mutation.md`; aggregate: `./build/rc_mutscore total results/mutation/*` (`results/mutation/total.md`).

What is mutated: the simulators' step functions and their force, integration, controller and signal-logic helpers. Not mutated: the scenario sampler, JSON and validation code,
the seeded-bug switches (dead code on the clean path), and the energy helpers the rule checker itself calls (mutating the oracle would mutate the test, not the system under test).

Classes (a violation counts only on a scenario where the unmodified simulator passes; the unmodified rigid-body simulator has one known violation in the pool, its heavy-light squeeze weakness):
- **unchanged**: identical traces on all 1000 scenarios (equivalent, or never triggered by random scenarios);
- **crashed or timed out**: the mutant segfaults or runs past 10 minutes;
- **changed, no declared rule fires**: the trace differs but every declared rule holds;
- **killed by rules**: some declared rule is violated.

## Result (160 mutants; final run, with the metamorphic arm described below)

| domain | mutants | unchanged on the pool | crashed or timed out | changed, no declared rule fires | killed by hand-written rules | of the changed: killed by hand-written rules | of the changed: killed by model-written invariants | killed mutants that random testing finds in every step-budget window | of the changed: killed by metamorphic relations | of the changed: hand-written rules or metamorphic | of the changed: any of the three oracles |
|---|---|---|---|---|---|---|---|---|---|---|---|
| orbit | 40 | 8 | 7 | 5 | 20 | 20/25 | 14/25 | 19/20 | 8/25 | 20/25 | 20/25 |
| cartpole | 40 | 5 | 0 | 20 | 15 | 15/35 | 12/35 | 15/15 | 8/35 | 16/35 | 16/35 |
| traffic | 40 | 2 | 0 | 31 | 7 | 7/38 | 2/38 | 7/7 | 0/38 | 7/38 | 7/38 |
| rigid2d | 40 | 15 | 1 | 13 | 11 | 11/24 | 8/24 | 9/11 | 8/24 | 14/24 | 15/24 |
| **all** | 160 | 30 | 8 | 69 | 53 | 53/122 | 36/122 | 50/53 | 24/122 | 57/122 | 58/122 |

(The "step-budget windows" are consecutive stretches of the scenario pool of at least 300000 simulated steps, the random baseline's budget; a mutant is "found in a window" if its scenarios
include a violation. 5 to 7 windows per domain. Metamorphic relations are evaluated on the first 300 scenarios of the pool.)

An earlier run of the same locked script, without the metamorphic arm, gave 54 rule-killed mutants and 6 orbit crashes or timeouts instead of 53 and 7: one slow orbit mutant that finished in that run hit the 10-minute
per-mutant timeout in the final run, which does extra work per scenario. The final run is the one reported; the difference is one mutant.

## What this shows

- **Sampled bugs are easy for random testing.** Of the 53 mutants a declared rule can catch, random testing finds 50 in every one of the budget windows. There is almost no headroom for a model-driven
  search on this kind of bug; I therefore did not spend API credits running the agent on mutants, and the earlier finding (the agent does not beat random on search) is consistent with it.
- **The limit is the oracle, not the search.** Of the 122 mutants that change behavior, 69 (57%) violate no declared rule. The hand-written rules catch 53 of the 122 (43%), the model-written invariants from
  `docs/SPEC_RULES.md` catch 36 (30%), and the metamorphic relations catch 24 (20%). Together the three oracles catch 58 of 122 (48%); the metamorphic relations add only 4 mutants beyond the hand-written rules (cartpole 1, rigid bodies 3), and the model-written invariants add 1 more. **About half of the behavior-changing mutants (64 of 122) are still invisible to every oracle here, and in traffic 31 of 38 are.**
- Metamorphic relations are not a substitute for declared rules: **perm**, **mirror** and **refine** mostly catch the same mutants the conservation rules already catch (orbit: 8 killed, 0 new), and for traffic only prefix and repeat apply, which found nothing.
- 30 of 160 (19%) are unchanged on 1000 scenarios, and 8 crash or hang (any harness notices those; they are not rule findings).

## Caveats

- One sample of 40 mutants per domain and one seed; the operator set is small (no statement deletion, no data-structure mutations). Counts of equivalent mutants are upper bounds on equivalence: a mutant
  unchanged on 1000 random scenarios may still be triggered by a rare input.
- "Changed, no declared rule fires" is not the same as "a real bug": some of those mutants alter numerics inside the rules' tolerances and are harmless. I did not triage the 69 by hand.
- The hand-written rules and the model-written invariants were both written before this benchmark and were not changed after seeing it. The metamorphic set and tolerances were fixed before the final run (see below).
- Differential testing against an independent implementation would see every behavior change by construction when the reference is the unmodified simulator, so it is not scored here; against an independent
  implementation (as in `docs/EXTERNAL_ENGINE.md`) it is a separate and unmeasured question.

## Second arm: metamorphic oracles (design fixed before any mutant was run with them)

Because the first arm showed the oracle is the limit, I added relations between runs that must hold for any correct simulator and that know nothing about any bug
(`src/rules/metamorphic.cpp`): **prefix** (a run of N/2 steps equals the first half of the N-step run, exactly; all domains), **repeat** (the same scenario twice gives identical
traces, exactly; all domains), **perm** (reversing the body order does not change the physics; orbit), **mirror** (reflecting x reflects the trajectory; orbit, cart-pole without noise, rigid
bodies without a controller) and **refine** (halving the step gives the same early trajectory; orbit and cart-pole). Traffic gets only prefix and repeat. They are evaluated on the first 300
scenarios of the pool. Tolerances were set on the unmodified simulator only: five times the largest clean deviation over a calibration pool (seeds 500000+) and the clean evaluation pool, rounded up
to one significant figure. The first attempt (twice the clean maximum of a 300-scenario pool) gave a false positive on the evaluation pool and was loosened before any mutant was run with it;
the final tolerances give 0 failures on 1000 clean scenarios in every domain. Calibration also showed that perm and refine do not hold on rigid-body contacts even for the correct simulator
(deviations 9.8 and 17.7: contact resolution legitimately depends on order and amplifies the step change), so they are not applied there.

## Reproducibility note

Regenerating the locked mutants was checked against the committed benchmark: cart-pole and traffic reproduce byte for byte; orbit reproduces the same 40 mutations at the same lines (the only difference is four lines outside the mutated ranges, the later `clamp_int` fix, which does not change behavior for in-range scenarios); the
rigid-body mutants were generated from a `world.cpp` that later changed, so the pinned source is `data/mutation_base/rigid2d_world.cpp`, and `scripts/run_mutation_all.sh` uses it (`rc_mutgen` on it reproduces the locked `index.json` and all 40 mutant sources exactly).
