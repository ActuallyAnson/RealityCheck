# Invariant mining: an oracle with no model and no hand-written rules

The mutation benchmark (`docs/MUTATION.md`) found that the oracle, not the search, is the bottleneck: 69 of 122 behavior-changing mutants violate no declared rule. This adds an oracle that needs neither a model nor a person to write rules, and that works on any simulator
that exposes named per-step signals (the four built-in domains, a model card, an exec engine). `src/spec/miner.{hpp,cpp}`; `rc_mine` runs it on any engine.

## Method

From a pool of runs of the **system under test itself**, propose candidate invariants over its named signals: range bounds, conserved quantities, conserved sums and differences of signal pairs, monotone signals, mutually exclusive flags, and bounded step-to-step change. Tolerances are set from what the pool
showed (bound margin = a fraction of the observed range; conserved tolerance = a factor times the largest observed drift; step bound = a factor times the largest observed step change). A run is flagged when it breaks an invariant mined from other runs of the same simulator.

**What it can and cannot find (stated before any result):** a miner learns "normal" from the system under test. It can flag behavior that departs from what the mining pool showed (a rare regime, an outlier run). It cannot flag a bug present in every run of the mining pool, because that bug looks normal.
So the expectation is a modest number of mutants, concentrated in rare-condition changes, and nothing for always-on errors; the interesting number is how many of the mutants that *none* of the other oracles catch it catches.

## Protocol (fixed before the run)

- Mining pool: 300 scenarios, seeds 900000+, simulated by the build under test (the mutant mines from its own runs; it never sees the unmodified simulator).
- Evaluation pool: the benchmark's 1000 scenarios, seeds 700000+ (unchanged).
- Tolerance factors: three numbers per domain, set once on the **unmodified** simulator by `scripts/mine_calibrate.sh`, on 1000 other scenarios (seeds 800000+): the first rung of a fixed ladder with zero false positives is used. This is the one place the miner consults a known-good simulator, as the metamorphic
  tolerances did; it sets three numbers and does not define any invariant. Result: `results/mutation/mine_factors.tsv`.
- Scored on the 160 locked mutants with `rc_minescore`: a mutant is killed when its trace differs from the clean one and some scenario fires a mined invariant that the clean simulator's own mined invariants do not fire on.
- False positives: clean evaluation-pool scenarios on which a mined invariant fires (must be 0 to be usable as an oracle).
- No parameter or template is changed after seeing mutant results; if a result is poor it is reported as it is.
