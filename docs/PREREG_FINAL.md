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
