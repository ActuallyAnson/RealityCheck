# Triage: the fuzzer finds, the model explains

Random testing finds most of the seeded failures cheaply (`docs/GUIDED_SEARCH.md` shows a smarter search did not beat it), so the model is used
where a program is weakest: working out **why** a found failure happens. `rc_triage` (lock: `data/eval_lock_triage.json`) takes, per bug and seed,
the first failing scenario that plain random sampling finds (no model involved; a failure the clean simulator also shows is discarded), and then
tries to localize the cause two ways:

- **agent v2 in triage mode:** it is handed the failing scenario and may not hunt; it shrinks, inspects, runs one counterfactual change that makes
  the failure go away, and submits a report that is checked against the recording (frames shown, low reasoning effort, 25 turns, unchanged from the ablation);
- **no-model localizer:** shrink, then try single-field edits (x0.9, x1.1, x0.8, x1.25, x0.5, x2, 0, boolean flips; not `seed` or `steps`) and take the
  smallest relative edit that makes the failure disappear.

Both are scored the same way: does the changed field belong to the bug's hidden trigger fields (`data/ground_truth.json`, harness only)? No keywords.

## Result (5 seeds x 22 bugs; the fuzzer found a failure in 83 of 110 runs; agent cost $3.65, about $0.044 per run)

Full per-bug table: `results/triage/summary.md` (regenerate with `./build/rc_triage summarize --lock data/eval_lock_triage.json --out results/triage`).

| | on the 83 failures |
|---|---|
| agent: report accepted (cited values match the recording, counterfactual really passes) | 81 [0.92-0.99] |
| agent: cause localized | **52 [0.52-0.72]** |
| no-model localizer: cause localized | **35 [0.32-0.53]** |

Paired, on the same failures: the agent localized where the baseline did not 26 times, the baseline where the agent did not 9 times (exact sign test p = 0.006).
The agent's counterfactual may change two fields and the baseline's one, which could favour the agent, so restricted to the 70 reports whose counterfactual changed
exactly one field: agent 46, baseline 30, agent-only 23, baseline-only 7, p = 0.005.

By bug (5 seeds each), where it matters: the agent is much better on o01 (5 against 0), c01 (5 against 2), b05 (5 against 1), b08 (5 against 2), t01 (5 against 1);
the baseline is better on o03 (5 against 1), b10 (4 against 1) and the two are close or tied on the rest (for example c03 0 against 1, o02 1 against 0, b07 0 against 0). A single draw of 5 seeds per bug is thin; the totals are the
claim, not the individual bugs.

## Caveats

- Bugs that random testing cannot find (b02, b06, b12, c02, and mostly b09) never reach triage: 27 of 110 runs. Triage helps once a failure exists, and says nothing about finding needles.
- All 22 bugs had been seen in earlier work, so none is fresh held-out; the agent prompt, tools and caps are those of the ablation and were not changed for this run.
- "Localized" means the counterfactual changed a trigger field. It does not check the written explanation (the keyword scorer is known to overstate; `docs/SCORER_AUDIT.md`).
- One baseline design. A stronger no-model localizer (for example ranking all removing edits, or two-field edits) might close part of the gap. The share of single-field
  removing edits that hit the cause is in the last column of the table (between 1% and 83% per bug; about a third overall), which is the chance rate for picking among them.
- Seeds label independent, nondeterministic model runs. The two agent failures to produce a valid report were one o03 run and one t02 run.

## Run-to-run variance (the model is not seeded)

The 34 failures found with fuzzer seeds 1 and 2 (the same failures as in the table above) were triaged by the agent twice more, with everything else unchanged (`data/eval_lock_triage_variance.json`, `results/triage_rep1`, `results/triage_rep2`;
about $2.8). `./build/rc_triage variance --lock data/eval_lock_triage_variance.json --dirs results/triage,results/triage_rep1,results/triage_rep2` prints `results/triage_variance.md`:

| repetition | failures | agent localized | agent valid reports |
|---|---|---|---|
| original run | 34 | 22 [0.48-0.79] | not shown here |
| repetition 1 | 34 | 22 [0.48-0.79] | 34 of 34 |
| repetition 2 | 34 | 20 [0.42-0.74] | 30 of 34 |

The no-model localizer localized 15 of the same 34. Per failure across the three runs: localized every time 19, never 11, **sometimes 4**. So the outcome is mostly a property of the failure, not of the model's randomness: about 12% of failures
flip between runs, and the aggregate rate moved by at most 2 of 34 (6 percentage points). In each repetition the agent's total is above the baseline's (22, 22, 20 against 15), but 34 failures alone are too few to show it (paired sign test p = 0.14 and 0.12 for the first two repetitions);
the significance of the full evaluation (p = 0.006) comes from its 83 failures. Repetition 2 produced fewer valid reports (30 of 34), which counts against localization.

## Portability: older models of the same family

Seed-1 failures only (17 failures with a fuzzer hit, lock `data/eval_lock_triage_portability.json`), same prompts, same price, run on `muse-spark-1.2` and `muse-spark-1.1` (`configs/hosted_1_2.json`, `configs/hosted_1_1.json`; reproduce with `rc_triage run --lock ... --config ...`).

| model | valid reports | cause localized | no-model localizer | agent-only / baseline-only | sign test p |
|---|---|---|---|---|---|
| muse-spark-1.3 | 16/17 | 11/17 | 6/17 | 6 / 1 | 0.125 |
| muse-spark-1.2 | 15/17 | 9/17 | 6/17 | 7 / 4 | 0.55 |
| muse-spark-1.1 | 15/17 | 9/17 | 6/17 | 6 / 3 | 0.51 |

Every model localizes at least as often as the baseline in raw counts, but with 17 failures none of these is significant by itself; the claim that rests on a test remains the 83-failure run. What the portability run supports is narrower: the pipeline works unchanged on
older models with comparable validity (15 to 16 of 17), and the older ones localize slightly less often (9 against 11). Cost $0.66 and $0.65. Summaries: `results/triage_spark12/summary.md`, `results/triage_spark11/summary.md`.

### Portability on the full set

The seed-1 subset above was too small to test anything. The same full locked evaluation (83 failures, `data/eval_lock_triage.json`) was then run on the two older models:

| model | valid reports | cause localized | no-model localizer | agent-only / baseline-only | sign test p | 1-field restriction p | cost |
|---|---|---|---|---|---|---|---|
| muse-spark-1.3 | 81/83 | 52/83 | 35/83 | | 0.006 | 0.005 | $3.65 |
| muse-spark-1.2 | 81/83 | 49/83 | 35/83 | 25 / 11 | 0.029 | 0.007 | $2.97 |
| muse-spark-1.1 | 82/83 | 48/83 | 35/83 | 25 / 12 | 0.047 | 0.041 | $2.41 |

The finding replicates on both older models of the same family: each localizes the cause in 48 to 49 of 83 failures against 35 for the no-model localizer, with p below 0.05 on each. The effect is slightly smaller on older models (48 to 49 against 52), as expected. Caveats: three tests were run on one failure set, so the p-values are not
independent evidence and the 1.2 and 1.1 values are borderline (0.03 to 0.05) taken alone; the three models are one family, so this is not evidence about other model families. Summaries: `results/triage_spark12_full/summary.md`, `results/triage_spark11_full/summary.md`.

## Open-weight model (Muse Spark Open, via Baseten)

The same locked 83 failures, same prompts and procedure, on the hackathon's open-weight model (`meta-models/muse-spark-open-1026`, `configs/baseten_open.json`; `results/triage_open/summary.md`). Cost shown in the summary ($1.91) is computed with the hosted model's rates as a proxy; the real spend comes out of the Baseten credits.
**Valid reports 79 of 83; cause localized 44 of 83 against 35 of 83 for the no-model localizer; paired sign test p = 0.18 (one-field restriction: 36 against 25, p = 0.052).**
Same direction as the hosted models (49 to 52 of 83), but not significant on its own, and the localization count is lower than every hosted model. One run per failure; the model is not seeded. The pricing field is a proxy, so the per-run budget caps are approximate.


## Replication on fresh failures (pre-registered, 2026-10-29)

The p = 0.006 above comes from one set of 83 failures; re-runs of the same failures (the other p-values) cannot be pooled with it. To test it properly, the lock `data/eval_lock_triage2.json` (committed before the run, `docs/PREREG_FINAL.md`) repeats the evaluation with fuzzer seeds 6 to 10: same 22 bugs, same fuzzer budget, same agent (muse-spark-1.3, agent v2 triage mode), same scoring and same no-model localizer, but **new failing scenarios** (the bugs are not new). A stronger no-model baseline (the field that the most different single-field edits fix) was declared before the run and also scored; on the original 83 it localizes 32 (worse than the original baseline's 35).

| | failures | valid reports | agent localized | no-model localizer | stronger baseline |
|---|---|---|---|---|---|
| original (seeds 1-5) | 83 | 81 | 52 | 35 | 32 |
| **replication (seeds 6-10)** | 88 | 85 | **53** | 37 | 37 |

Replication, paired on the same failures: agent localized and the baseline did not 29 times, baseline and not agent 13; exact sign test p = 0.0195 two-sided (about 0.01 one-sided, pre-registered alpha 0.05 one-sided: **met**). Against the stronger baseline: 25 against 9, p = 0.009. Restricted to one-field counterfactuals: 44 against 31, p = 0.035. Cost $4.02. Pooling the two sets (independent failures): 171 failures, agent 105 (61%) against 72 (42%), discordant pairs 55 against 22.
Limits, unchanged: the same 22 seeded bugs (planted by me) as before; localization is scored by hidden trigger fields, not by a human reading the explanation; one model family (the open-weight model gave 44 of 83, p = 0.18, same direction); the agent's rate fell a little (63% to 60%) and the baseline's rose (42% to 42%), so the margin is about 18 points, not 21. Raw: `results/triage_rep/`.
