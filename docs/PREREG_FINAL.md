# Pre-registration for the final week (written 2026-10-29, before any of these runs)

Everything below is fixed before the corresponding run. Anything decided after seeing results is labelled "post hoc" in the doc that reports it.

## REBOUND TRACE truth oracle, rerun without softening (declared before running)

Diagnostic already run (no model, `examples/external/frame_rms.cpp`, 200 scenarios of `results/realproj/rebound_trace_seed11_req.jsonl`, frame 100): against the Boost.odeint Bulirsch-Stoer truth, REBOUND IAS15 has median RMS position error 6e-12 and REBOUND BS 3e-10, so the truth model is right; leapfrog (which applies the softened force) 0.30; WHFast 1.52; TRACE pre-fix and post-fix 1.14. WHFast and TRACE take Kepler steps that do not include the softening, so with softening 0.3 they solve a different problem than the truth. The earlier "3 vs 7, p=0.34" result therefore compared both builds against a reference they cannot match; it stays reported, but it was not a fair test of the fix.

Rerun: same 200 scenarios, softening 0 for the truth and for both builds (`RC_SOFTENING=0`). Gate first: REBOUND IAS15 must agree with the truth at frame 100 with median RMS error below 1e-6, otherwise the truth is invalid and the rerun is void. Truth runs that do not finish within 120 s per scenario are dropped for all arms and counted. Same metric, same scoring rule, same verdict as the earlier oracle (scored where pre and post differ at frame 100; two-sided paired sign test; the oracle distinguishes the fix if p < 0.05 with post closer in the majority). Report the counts whatever they are.

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

## Addendum 2026-10-29, section 2 made concrete (agent versus random), locked before any run
- Headroom targets are the seeded bugs where random testing found the bug in at most 1 of 5 earlier seeds at the 300,000-step budget: b02, b06, b09, b12 (rigid bodies) and c02 (cart-pole). (The exact-baseline table in `results/randbase/table.md` shows the sampled mutants have almost no headroom, so they are not used.)
- Agent arm: hybrid agent v4, muse-spark-1.3, 20 fresh seeds (5001-5020) per target (`data/eval_lock_headroom_*.json`). Random arm: 60 fresh seeds (6001-6060) per target at the same step budget (`*_random.json`), so random's rate is measured, not assumed.
- Test: one-sided Fisher exact test per target (agent detection > random detection, alpha 0.05), then an exact sign test over targets. If the agent does not win, the claim is that on these targets search shows no reliable advantage over random testing, with the counts.
- "Detected" means the agent's own run produced a rule violation on the seeded-bug simulator (engine-confirmed), as in the earlier tables; report validity is reported alongside.

## Addendum 2026-10-29: boundary-value sampling vs random (declared before the first run)

Why: the agent did not beat random on the five low-headroom bugs because its strategy collapses into random sampling (`docs/HYBRID.md`). A model-free alternative that a tester could use instead: random scenarios plus boundary cases. `rc_extreme` (`src/eval/extreme_main.cpp`): the "extreme" arm alternates untouched random scenarios with scenarios whose numeric fields are each pushed with probability 0.3 to the most extreme value that still validates. Both arms get 300000 steps per run.
Bugs: rigid2d b02, b06, b09, b12 and cart-pole c02 (the five low-headroom bugs). Seeds 8000..8039 (40 per bug and arm, fresh: earlier runs used other seeds). One setting, no tuning, no second run on the same seeds.
Primary: pooled detections over the five bugs, exact one-sided Fisher test (extreme > random), alpha 0.05. Secondary: per bug counts. Whatever comes out is reported in `docs/HYBRID.md`.

## Addendum 3 2026-10-29: agent v5 (boundary option) vs random (declared before the first run)

Agent v5 = v4 plus `sample_random(boundary: true)` and one prompt paragraph recommending it (`src/tools/tools.cpp`, `src/agent/agent.cpp`, `src/sim/extreme.hpp`). Locks `data/eval_lock_headroom5_{rigid2d,cartpole}.json`: the same five bugs (b02, b06, b09, b12, c02), muse-spark-1.3, 20 fresh seeds (7001-7020) per bug, 300000 steps. Comparator: the existing random arm (60 seeds per bug, 17/300 detections) and the v4 agent arm (5/100).
Primary: pooled detections of v5 over the five bugs (100 runs) against the random arm's 300 runs, exact one-sided Fisher, alpha 0.05. Also reported: per bug, valid reports, cost, and the same pooled test without b12 (the boundary-triggered bug). One run per seed; no tuning on these seeds. The prompt and tool are frozen at the commit that contains this addendum.
