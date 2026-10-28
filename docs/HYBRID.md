# Hybrid agent (v4): a random sampler inside the tester

**Motivation.** The first evaluations showed the model-only agent losing to random testing on the traffic domain (t02 1/5 and t03 3/5 against 5/5 for random at the same step budget, `docs/TRAFFIC_DOMAIN.md`). The model's own scenarios are less varied than a random generator's.
**Change.** Agent v3 and v4 add one tool, `sample_random`: it runs scenarios from the same generator the random baseline uses, spends the same step budget, and stops at the first violation. v4 differs from v3 only in the batch cap (400 instead of 100) and one prompt paragraph recommending large batches. v1 and v2 are untouched (their prompts and tool lists are byte-identical, so recorded runs still replay). Everything else, including checked counterfactual reports, is the v2 agent.
The sampler's stream is seeded by the evaluation run's seed, exactly as the random baseline is, and successive calls continue the stream. Tests: `sample_random tool` in `rc_tests`.

## Results (muse-spark-1.3, 5 seeds per bug, the locked sets, same step budget as the baseline)

| domain | agent | detected | valid report | cost |
|---|---|---|---|---|
| traffic (t01 t02 t03) | v1, model only | 9/15 (5, 1, 3) | 9/15 | $1.37 |
| traffic | random baseline | 15/15 | no report | n/a |
| traffic | **v4 hybrid** | **15/15 (5, 5, 5)** | **15/15** | $0.74 |
| orbit (o01 o02 o03) | v1 | 15/15 | 15/15 | $1.06 |
| orbit | v2 | 15/15 | 15/15 | $1.22 |
| orbit | v4 hybrid | 15/15 | **11/15 (3, 3, 5)** | $1.01 |
| cart-pole (c01 c02 c03) | v1 | 12/15 (5, 2, 5) | 12/15 | $1.45 |
| cart-pole | random baseline | 10/15 (5, 0, 5) | no report | n/a |
| cart-pole | v4 hybrid | 12/15 (5, 2, 5) | 12/15 | $0.40 |

Raw tables: `results/traffic_agent_hybrid/summary.md`, `results/orbit_agent_hybrid/summary.md`, `results/cartpole_agent_hybrid/summary.md`, `results/orbit_agent_v2/summary.md`; the v1 and baseline rows are the earlier committed tables.

## What this says, and the limits

- **On traffic the hybrid removes the loss to random testing** (15/15 against 9/15) and still returns a validated, counterfactual-checked report for each failure (15/15), which the random baseline cannot. This is not "the model beats random": on the runs where the model samples first, its stream is the baseline's stream (same seed), so its detection equals the baseline's by construction. The honest claim is that the hybrid ties random on finding and adds the explanation.
- **It is not free.** On orbit it found every failure but produced a valid report in 11 of 15 runs against 15 of 15 for v2 (two-sided Fisher exact p = 0.10, not significant at this sample size). The four lost runs ended at the turn limit with a rejected report: my guess, not verified, is that a randomly sampled failing scenario is a large one and the shrink-then-counterfactual procedure is harder than on the small scenarios the model builds itself. I did not tune the prompt for this.
- **Cart-pole c02 stays at 2/5**: the hybrid does not find what neither random testing (0/5) nor the model (2/5) finds more often. It did not help or hurt there.
- The rigid-body locked set was not re-run with the hybrid. One model, 5 seeds per bug: intervals are wide.

## Two flawed attempts that I made and fixed (kept in the repository, labelled)

The first version of `sample_random` took a `seed` argument chosen by the model. Two traffic runs used it (`results/traffic_agent_v3_modelseed`, 12/15 detected; `results/traffic_agent_v4_modelseed_a`, 11/15): the t03 runs chose seed 0 or 1, so their streams overlapped; in `traffic_agent_v4_modelseed_b` (the same tool with the stream seeded differently) all five t03 runs chose seed 1 and drew the same 191 scenarios, none of which triggered the bug, so five runs were one sample, not five (0/5 on t03).
That was a design error of mine, not a property of the model: runs that should be independent shared a stream. The fix (seed from the run's seed, as the random baseline does; the model no longer chooses a seed) was made after seeing those results, so it is a post-hoc correction. The numbers in the table above are from the corrected tool only.
