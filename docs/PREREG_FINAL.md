# Pre-registration for the final week (written 2026-10-29, before any of these runs)

Everything below is fixed before the corresponding run. Anything decided after seeing results is labelled "post hoc" in the doc that reports it.

## Finding that shapes the plan (computed, no model: `results/randbase/table.md`)
Random testing with the hand-written rules already finds almost every mutant the rules can see: at a budget of 30 scenarios only 4 of 160 sampled mutants have a random-testing hit probability below 0.5. So the mutation benchmark has no room for a search agent to beat random testing. The unsolved part is the oracle gap (behavior changes that no rule sees), not search.

## 1. Oracle gap (reference oracle for traffic and rigid2d)
- Same method as `docs/REFERENCE_ORACLE.md`: the model sees the description (rules section removed) and an interface declaration, writes one-step physics; a sandboxed harness compares.
- Gate 1 (clean): 0 flagged steps on 1000 fresh clean scenarios per domain (seeds 910000+). If it fails, that domain is not scored and the failure is reported.
- Gate 2 (tolerances): fixed before the mutant run; no change after seeing mutant results.
- Success: the share of the 122 changed mutants (orbit, cart-pole, traffic, rigid2d: recount after this run, same sampling) that some oracle catches rises from 60% to at least 70%. If not, the claim is the exact count and the domains where it works. Never "gap closed".
- Second model draw per domain to address the one-draw caveat; both draws reported.

## 2. Agent versus random
- Only on targets where random testing has headroom: the seeded bugs b02, b06, b09, b12 and c02, plus any sampled mutant with random hit probability below 0.5 at the budget.
- Budgets of 10, 30 and 100 scenarios, equal for both arms. Hybrid agent v4. 15 seeds per target.
- Test: exact binomial against the analytic random rate per target and budget, one-sided, alpha 0.05, then a sign test across targets.
- If the agent does not win, the claim is "no headroom on sampled bugs; value is triage and verified reports".

## 3. Triage replication
- Fresh failures from fuzzer seeds 6 to 10 (not used before). Same pipeline, same scoring, same baseline as `docs/TRIAGE.md`, plus a stronger no-model baseline declared before the run.
- Test: one-sided paired sign test against both baselines, alpha 0.05. A miss is reported as a failed replication next to the original p = 0.006.

## 4. Real historical bugs
- Only with the user's approval to compile third-party code. Commit list locked by hash before any oracle runs. Fewer than 5 usable commits: dropped and said so.

## 5. UI
- Scan, failure list, oracle-gap matrix in the existing viewer. No demo, no recordings.

## Addendum 2026-10-29 (traffic reference), written before any traffic mutant was scored
- The traffic reference needs a behavioural specification precise enough to re-implement, so `src/eval/ref_traffic_spec.txt` states the controller in full (like the orbit description, which states the force law). A reference built from it is a **conformance check of the implementation against its specification**, not a discovery of unspecified behavior. The spec was validated with a reference the author wrote from the spec text (0 flags on 94,643 clean steps).
- Draw 1 (spec v1) **failed the clean gate** (49,583 of 466,898 steps flagged): the model read an ambiguous sentence as allowing two phase changes in one second. The sentence was reworded (spec v2) and two fresh draws were taken that never saw draw 1. This is a post hoc spec change, made after seeing a failed gate; the gate and tolerances were not changed.
- Draws 2 and 3 passed the gate. Then a rare clean case appeared on the mutant evaluation's seeds: seconds with emergency vehicles active in both directions at once, where the simulator lets the first-listed event decide and the spec is silent. Those seconds are now **not judged** (post hoc harness change; also applied to the author reference). Re-checked on 1000 fresh clean scenarios (seeds 930000+): 0 flags for both draws.
- **Headline draw: draw 2** (the first draw that passed the gate), declared before any mutant was scored. Draw 3 is reported as a replication, not used to pick the better number.
- The judge compares the phase, all four queues and all four red-waits each second, trying every combination of "a vehicle arrived" on the approaches that can receive one (arrivals are not recorded), and flags only if even the best-matching combination differs.
