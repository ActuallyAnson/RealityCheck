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
the baseline is better on o03 (5 against 1), b10 (4 against 1) and ties or trails on a few others (c03, o02, b07). A single draw of 5 seeds per bug is thin; the totals are the
claim, not the individual bugs.

## Caveats

- Bugs that random testing cannot find (b02, b06, b12, c02, and mostly b09) never reach triage: 27 of 110 runs. Triage helps once a failure exists, and says nothing about finding needles.
- All 22 bugs had been seen in earlier work, so none is fresh held-out; the agent prompt, tools and caps are those of the ablation and were not changed for this run.
- "Localized" means the counterfactual changed a trigger field. It does not check the written explanation (the keyword scorer is known to overstate; `docs/SCORER_AUDIT.md`).
- One baseline design. A stronger no-model localizer (for example ranking all removing edits, or two-field edits) might close part of the gap. The share of single-field
  removing edits that hit the cause is in the last column of the table (between 1% and 83% per bug; about a third overall), which is the chance rate for picking among them.
- Seeds label independent, nondeterministic model runs. The two agent failures to produce a valid report were one o03 run and one t02 run.
