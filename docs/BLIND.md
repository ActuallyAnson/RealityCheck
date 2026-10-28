# Blind bug sets

Every bug in this repository was planted or sampled by me, so none of the evaluations is blind. This is the mechanism for one that is: a person who is not the tester writes the bugs, the set is locked by hash before any tool runs on it, and the oracles are scored on it.

1. **Planter** (anyone but the evaluator; ideally someone who has not read the oracles' code): make a copy of a simulator source (for built-in domains, `src/sim/orbit.cpp`, `cartpole.cpp`, `traffic.cpp`, or the pinned `data/mutation_base/rigid2d_world.cpp`), introduce a realistic bug, and save each variant as its own `.cpp` file in a directory
   (they link against `librc_core.a` exactly as the mutants do). Do not show the variants to the evaluator.
2. **Lock**: `scripts/blind_lock.sh DIR DOMAIN` writes `data/blind/manifest_DOMAIN.json` with the SHA-256 of every file and the commit; commit and push it before anything runs.
3. **Evaluate**: `scripts/blind_eval.sh DIR DOMAIN` refuses any file whose hash is not in the manifest, then reports, per variant, how many scenarios differ from the clean simulator and how many fire a hand-written rule or a mined invariant. It is a harness for the oracles; the agent runs the same way against a variant build.
4. **Reveal**: the planter's descriptions are published next to the results.

Status: the mechanism is built and its refusal path is exercised in `scripts/blind_selftest.sh`; **no blind set exists yet**, so there are no blind results. It needs another person.
