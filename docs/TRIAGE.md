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
