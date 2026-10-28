# Blind bug sets

Every bug in this repository was planted or sampled by me, so none of the evaluations is blind. This is the mechanism for one that is: a person who is not the tester writes the bugs, the set is locked by hash before any tool runs on it, and the oracles are scored on it.

1. **Planter** (anyone but the evaluator; ideally someone who has not read the oracles' code): make a copy of a simulator source (for built-in domains, `src/sim/orbit.cpp`, `cartpole.cpp`, `traffic.cpp`, or the pinned `data/mutation_base/rigid2d_world.cpp`), introduce a realistic bug, and save each variant as its own `.cpp` file in a directory
   (they link against `librc_core.a` exactly as the mutants do). Do not show the variants to the evaluator.
2. **Lock**: `scripts/blind_lock.sh DIR DOMAIN` writes `data/blind/manifest_DOMAIN.json` with the SHA-256 of every file and the commit; commit and push it before anything runs.
3. **Evaluate**: `scripts/blind_eval.sh DIR DOMAIN` refuses any file whose hash is not in the manifest, then reports, per variant, how many scenarios differ from the clean simulator and how many fire a hand-written rule or a mined invariant. It is a harness for the oracles; the agent runs the same way against a variant build.
4. **Reveal**: the planter's descriptions are published next to the results.

Status: the mechanism is built and its refusal path is exercised in `scripts/blind_selftest.sh`. **A human-planted blind set does not exist yet** (it needs another person). A *model-planted* set does exist and was run through this protocol; it is described below.

## A model-planted set (the planter is a model, not a person)

`rc_plant` shows a model only the simulator's source (no rules, no oracles, no tests) and asks for realistic bugs as exact line edits, which it applies mechanically (anything with a preprocessor line or an operating-system call is refused). The planter was muse-spark-1.2 (the tester model in the rest of the repository is muse-spark-1.3); 10 bugs were requested per simulator, cost $0.09 in total.
Protocol as above: the 39 variants (one refused edit set in cart-pole) were hashed and the manifests committed (`17520b7`) before any oracle ran; the planter's descriptions were published only after the results (`data/blind/planted/*/planted.json`). I chose neither the bugs nor their number, and did not read them before the lock.
`scripts/blind_eval.sh` ran every variant on 500 scenarios (seeds 700000+); `scripts/blind_summary.sh` writes the table.

| domain | variants | changed behavior | a hand-written rule fires | a mined invariant fires | changed, no hand-written rule fires | of those, mined invariant fires |
|---|---|---|---|---|---|---|
| orbit | 10 | 9 | 7 | 0 | 2 | 0 |
| cartpole | 9 | 7 | 2 | 0 | 5 | 0 |
| traffic | 10 | 9 | 0 | 0 | 9 | 0 |
| rigid2d | 10 | 9 | 5 | 1 | 4 | 0 |
| total | 39 | 34 | 14 | 1 | 20 | 0 |

- **It reproduces the oracle gap with bugs I did not choose.** The hand-written rules fire on 14 of the 34 variants that change behavior (41%; 43% on the author-sampled mutation benchmark), 20 changed behavior with no rule firing (59%), and the mined invariants add nothing to them (the one mined hit is a variant a rule also catches). Traffic is the extreme: 0 of 9.
- **Limits, plainly.** The planter is a model of the same family as the tester, not an independent person. The hand-written rules and the miner were written or tuned by me, so this set is blind to the author's choice of bugs, not to the author's choice of oracles. A model planter favours simple edits (a flipped sign, a swapped argument, a constant) and some of its "bugs" are in serialization or validation code that does not change the physics.
  Five variants showed no behavior change in 500 scenarios (equivalent or too rare to see here); they are counted in the first column only. This run evaluates the hand-written rules and the mined invariants; model-written invariants and metamorphic relations were not run on this set.
- Raw rows: `results/blind/eval_*.txt`.
