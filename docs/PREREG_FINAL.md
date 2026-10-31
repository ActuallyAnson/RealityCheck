# Pre-registration for the final week (written 2026-10-29, before any of these runs)

Everything below is fixed before the corresponding run. Anything decided after seeing results is labelled "post hoc" in the doc that reports it.

## REBOUND TRACE truth oracle, rerun without softening (declared before running)

Diagnostic already run (no model, `examples/external/frame_rms.cpp`, 200 scenarios of `results/realproj/rebound_trace_seed11_req.jsonl`, frame 100): against the Boost.odeint Bulirsch-Stoer truth, REBOUND IAS15 has median RMS position error 6e-12 and REBOUND BS 3e-10, so the truth model is right; leapfrog (which applies the softened force) 0.30; WHFast 1.52; TRACE pre-fix and post-fix 1.14. WHFast and TRACE take Kepler steps that do not include the softening, so with softening 0.3 they solve a different problem than the truth. The earlier "3 vs 7, p=0.34" result therefore compared both builds against a reference they cannot match; it stays reported, but it was not a fair test of the fix.

Rerun: same 200 scenarios, softening 0 for the truth and for both builds (`RC_SOFTENING=0`). Gate first: REBOUND IAS15 must agree with the truth at frame 100 with median RMS error below 1e-6, otherwise the truth is invalid and the rerun is void. Truth runs that do not finish within 120 s per scenario are dropped for all arms and counted. Same metric, same scoring rule, same verdict as the earlier oracle (scored where pre and post differ at frame 100; two-sided paired sign test; the oracle distinguishes the fix if p < 0.05 with post closer in the majority). Report the counts whatever they are.

## REBOUND TRACE truth oracle, confirmation on new scenarios (declared before running)

The unsoftened rerun on seed 11 gave 11 vs 5 of 16 (p=0.21), which suggests but does not show an effect. Confirmation: 600 new scenarios from `examples/external/gen_crowded.cpp` (seed 12, same distribution as seed 11), softening 0, same builds, same gate (IAS15 vs truth median RMS error below 1e-6 at frame 100), same dropping rule, same metric. Primary test: two-sided exact sign test on the scored scenarios of this new set only, alpha 0.05; the fix is "distinguished" if p < 0.05 with the fixed build closer in the majority. The seed-11 result is not pooled into the primary test; a pooled count is reported as secondary. No other scenario set will be tried.

## Box2D ball-drop truth oracle (declared before scoring the restitution commits)

Truth: a frictionless circle dropped from rest onto the floor under gravity bounces with restitution e (`examples/external/ball_truth.cpp`; a bounce only if the impact speed exceeds the engine's restitution threshold, Box2D default 1 m/s); 1000 scenarios, seed 21 (`ball_truth gen 21 1000`; radius 0.2-0.6, height 3-10, e 0.3-0.95, no horizontal motion, horizon: second bounce plus 0.3 s), continuous collision on, substeps 4. Metric: RMS error of the centre height against the truth over all frames. Gate: both builds must have a median RMS error below 0.2 m, otherwise the truth does not describe the engine and the test is void.
Disclosure: before writing this I ran the tool once, as a sanity check of the truth, on the tunneling pair fb8b81f (parent vs fix): the two builds produced identical output in all 1000 scenarios, median error 0.074 m. That pair is reported as "not reached" and is not part of the test. Pairs tested here: 0217969 and 25b971d (restitution commits), as built by `scripts/build_box2d_hist.sh`, each separately. A pair "distinguishes" the commit if the two-sided exact sign test over scenarios where the builds differ has p < 0.025 (Bonferroni over the two pairs) with the post-commit build closer in the majority. Report the counts whatever they are. These commits redesigned restitution, so "closer to truth" is evidence about the new behavior, not necessarily a bug fix.

## Agent v5 on the open-weight model (declared before the first run)

The v5 agent exactly as frozen (commit 73966f5), the same locks (`data/eval_lock_headroom5_{rigid2d,cartpole}.json`: bugs b02, b06, b09, b12, c02, seeds 7001-7020), model `meta-models/muse-spark-open-1026` through Baseten (`configs/baseten_open.json`; the same model family as the hosted Muse Spark, different weights and host, not a different vendor). Output `results/headroom5_open_{rigid2d,cartpole}`. Primary: pooled detections over the five bugs against the existing random arm (17/300), exact one-sided Fisher, alpha 0.05; also reported per bug, valid reports, and next to the hosted v5 result (45/100). Budget caps $6 (rigid-body) and $3 (cart-pole) on the proxy pricing; if a cap stops a run early, the run is reported as incomplete with its seed count, not extended silently. One run per seed, no tuning.

## Second model-planted blind set, different planter (declared before planting)

Planter: `meta-models/muse-spark-open-1026` (Baseten), not the tester model and not the planter of the first set (muse-spark-1.2). `rc_plant` as in `docs/BLIND.md`: the planter sees only the simulator source, 30 bugs requested per simulator (orbit `src/sim/orbit.cpp`, cart-pole, traffic, rigid2d `data/mutation_base/rigid2d_world.cpp`), edits applied mechanically, refused edits dropped. Nothing is filtered by outcome: every applied variant is kept. Manifests (SHA-256 of every file, `MANIFEST=data/blind/manifest2_DOMAIN.json scripts/blind_lock.sh`) are committed before any oracle runs; the planter's descriptions are published only after the results (`data/blind/planted2/*/planted.json`). Evaluation: `scripts/blind_eval.sh` unchanged otherwise (500 scenarios, seeds 700000+; per variant: differs from clean, hand-written rules, mined invariants), table by `scripts/blind_summary.sh`. Reported next to the first set. This is still a model planter of the same family; it is not a human-planted set.

## Reference oracle on the second blind set (declared before running)

The already-stored reference programs (orbit and cart-pole draw 1, traffic draw 2, none regenerated or edited), the unchanged `scripts/refcheck_worker.sh` and tolerances (F = 0.02, K = 10), scored by `rc_refscore planted` against `results/blind2/eval_*.txt`, on all 90 orbit, cart-pole and traffic variants of the second blind set (300 scenarios, seeds 700000+). Reported: per domain, variants the reference catches among those that changed behavior, among those silent to the hand-written rules and mined invariants, and any flag on a variant that did not change behavior (false alarm). rigid2d has no reference and is not scored. No tuning after seeing results.

## Box2D thin-wall tunneling test (declared before running; replaces the tunneling attempt that used random scenarios)

The earlier random scenarios do not reach the tunneling fix fb8b81f in a way any oracle sees (0 tunneling with continuous collision on, in both builds). This test targets it: `examples/external/thin_wall.cpp`, seed 31, 1000 scenarios, one circle (radius 0.1-0.4) fired at 20-120 m/s at a static wall 0.02-0.2 m thick, no gravity, 30 steps of 1/60 s, substeps 4, continuous collision ON, parent d9b5732 (A) against fb8b81f (B), builds from `scripts/build_box2d_hist.sh`. Truth: a rigid body cannot end beyond an impenetrable wall it started in front of, so a scenario in which the circle's centre ends beyond the wall's far face counts as tunneling. Compared with an exact two-sided McNemar test on the discordant scenarios, alpha 0.05; the fix is "distinguished" if B tunnels in fewer scenarios with p < 0.05. Report A, B, both discordant counts and p whatever they are. If neither build tunnels in any scenario, the family does not reach the fix and that is reported as such.

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

## Addendum 2026-11-01: can rc_scan re-find the REBOUND IAS15 hang if the scenario family contains it? (declared before the run)

Why: the generic scan missed the IAS15 hang (`docs/SCAN.md`) because random starts are never symmetric. Question: given a start family that *contains* the symmetric structure, does the unmodified `rc_scan` hit the hang among its ordinary and boundary scenarios, and how often?
Not a blind find: the family `mirror_pairs` was written by me **after** knowing the hang (equal masses; bodies in pairs placed point-symmetrically about a centre with identical velocity inside a pair; for an odd count a central body carries the velocity that makes total momentum zero). The result measures re-discoverability, not discovery, and will be reported as that.
Setup: `examples/external/rebound_scan.cpp` gets a third layout choice `mirror_pairs`; REBOUND `262a9cce` built with its own flags; `rc_scan` as committed at this addendum, layout choices `random,mirror_pairs`, integrators `ias15,whfast,leapfrog,bs`, 600 scenarios, `RC_EXEC_TIMEOUT_S=5`, steps 50, seed family unchanged. One run, no tuning, no second run.
Primary: number of scenarios reported as `run failed` (a timeout), and whether any of them has `layout=mirror_pairs` with `integrator=ias15`. Secondary: findings in the other integrators. Whatever comes out is reported in `docs/SCAN.md`.
