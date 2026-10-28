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

## Result (160 mutants)

| domain | mutants | unchanged on the pool | crashed or timed out | changed, no declared rule fires | killed by hand-written rules | of the changed: killed by hand-written rules | of the changed: killed by model-written invariants | killed mutants that random testing finds in every step-budget window |
|---|---|---|---|---|---|---|---|---|
| orbit | 40 | 8 | 6 | 5 | 21 | 21/26 | 15/26 | 20/21 |
| cartpole | 40 | 5 | 0 | 20 | 15 | 15/35 | 12/35 | 15/15 |
| traffic | 40 | 2 | 0 | 31 | 7 | 7/38 | 2/38 | 7/7 |
| rigid2d | 40 | 15 | 1 | 13 | 11 | 11/24 | 8/24 | 9/11 |
| **all** | 160 | 30 | 7 | 69 | 54 | 54/123 | 37/123 | 51/54 |

(The "step-budget windows" are consecutive stretches of the scenario pool of at least 300000 simulated steps, the random baseline's budget; a mutant is "found in a window" if the
first scenarios in it include a violation. 5 to 7 windows per domain.)

## What this shows

- **Sampled bugs are easy for random testing.** Of the 54 mutants a declared rule can catch, random testing finds 51 in every one of the budget windows. There is almost no headroom for a model-driven
  search on this kind of bug; I therefore did not spend API credits running the agent on mutants, and the earlier finding (the agent does not beat random on search) is consistent with it.
- **The limit is the oracle, not the search.** Of the 123 mutants that change behavior, 69 (56%) violate no declared rule at all; in traffic it is 31 of 38. The hand-written rules catch 54 of the 123 (44%); the
  model-written invariants from `docs/SPEC_RULES.md` catch 37 (30%), and the two together add about one mutant to the hand-written rules (rigid bodies: 12 with both against 11).
  That makes "better rules" the most valuable thing a tool like this can add, and it matches why spec-to-rules and differential testing exist; the invariants as written do not yet deliver it.
- 30 of 160 (19%) are unchanged on 1000 scenarios, and 7 crash or hang (any harness notices those; they are not rule findings).

## Caveats

- One sample of 40 mutants per domain and one seed; the operator set is small (no statement deletion, no data-structure mutations). Counts of equivalent mutants are upper bounds on equivalence: a mutant
  unchanged on 1000 random scenarios may still be triggered by a rare input.
- "Changed, no declared rule fires" is not the same as "a real bug": some of those mutants alter numerics inside the rules' tolerances and are harmless. I did not triage the 69 by hand.
- The hand-written rules and the model-written invariants were both written before this benchmark and were not changed after seeing it.
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
