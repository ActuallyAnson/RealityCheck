# Repair loop: can a model repair a mutated simulator, and does "the rules pass" suffice to accept the repair?

The tester finds and explains failures; this closes the loop. For each mutant of the mutation benchmark (`docs/MUTATION.md`) that a declared rule catches, the model is shown the mutated source regions (the same line ranges that were mutated), one failing
scenario, the violated rule and the simulation description, and proposes replacement text for the regions. The mutants are not bugs I planted: they are mechanically sampled single changes (an operator or a constant) to code I wrote.
The engine, not the model, decides what happened (`rc_repair`, `src/eval/repair_main.cpp`):

| outcome | meaning |
|---|---|
| `restored` | every trace on a 300-scenario pool is bit-identical to the unmodified simulator's |
| `rules_pass_differs` | no declared rule or model-written invariant fails, but some trace differs from the unmodified simulator: **a check that only asks "do the rules pass" would accept this repair** |
| `still_violates` | a declared rule still fails where the unmodified simulator passes |
| `compile_failed`, `denied`, `timeout_or_crash`, `unparsed` | the patch did not build, was refused by the denylist, crashed or timed out, or the answer had no code block for every region |

Up to two attempts per mutant; on failure the model is told why (compiler errors, or the rule that still fails). The unmodified source is never shown to the model.

## Safety (model-written code is compiled and run on this machine)

- **Denylist before compiling**: a patch containing `#` (so no `#include` or `#define`), `system`, `popen`, `exec`, `fork`, file or stream classes, `socket`, `asm`, `dlopen`, `getenv`, `unlink`, `remove(`, `signal(`, `kill(`, `mmap`, threads, or `__` is refused.
- **Sandboxed run**: the compiled build runs under `sandbox-exec` with `scripts/repair_sandbox.sb`: no network, no file writes, no forking; plus a CPU limit (`ulimit -t 600`) and a wall-clock timeout. Checked offline with test programs before any live call: a write, a fork, a `connect` and a `bind` were each blocked, stdout still worked.
- **Pipeline checked offline** (`--selftest`, no model): feeding the unmodified code gives `restored`, the mutant unchanged gives `still_violates`, an uncompilable edit gives `compile_failed`, and a patch that tries `std::system("touch ...")` is `denied` and creates no file.
- Limits: the denylist is a token scan, not a proof; the sandbox profile is macOS-only and `sandbox-exec` is deprecated; compiling is not sandboxed (the compiler does not execute patch code). Do not run this on a machine with secrets you cannot lose without reading the profile.

## Setup (fixed before the live run)

Domains: orbit (`src/sim/orbit.cpp`, regions 74-146 and 164-189) and cart-pole (`src/sim/cartpole.cpp`, regions 94-139 and 147-197); every mutant of the locked benchmark that a declared rule catches (20 and 15). Model: `muse-spark-1.3`, low reasoning effort, 2 attempts,
$1.5 budget per domain. Pool: the first 300 scenarios of the benchmark pool (seeds 700000+).

## Results (live run, muse-spark-1.3, $0.59 in total)

| domain | mutants | restored | restored on attempt 1 | rules_pass_differs | other outcomes | cost |
|---|---|---|---|---|---|---|
| orbit | 20 | 20 | 19 | 0 | 0 | $0.26 |
| cart-pole | 15 | 14 | 14 | 1 | 0 | $0.33 |

34 of 35 repairs reproduced the unmodified simulator bit for bit on all 300 pool scenarios. The one exception (cart-pole `m001`, `*` changed to `/`, energy bound) passed every declared rule and invariant yet differed from the unmodified
simulator on all 300 scenarios: **a check that only asks "do the rules pass" would have accepted a wrong repair**; the bit-identity check rejected it. One sample is an existence proof, not a rate.

Reading this honestly: these are single-token mutants in a few dozen lines the model is shown, together with the failing scenario and the rule that broke, and the model has seen physics code like this; high restoration is expected. It does not show the model can repair bugs it
cannot localize from the shown region, nor bugs in code it has not been shown. Bit-identity on a 300-scenario pool is strong evidence but not a proof of equivalence. Raw rows: `results/repair/{orbit,cartpole}/repair.json`.

## Second run: larger and harder regions (design fixed before the run)

The first run showed the model a few dozen lines. This run adds the two larger domains, with no change to the method: traffic (`src/sim/traffic.cpp`, lines 112-253, 7 rule-caught mutants) and rigid-body (`src/sim/world.cpp`, lines 25-256, 11 rule-caught mutants; the model must find a one-token
change inside 232 lines of broad-phase, contact solver and integrator; all 11 mutated lines fall in this range, so the dispatch function of the benchmark's second range is not shown). Same model, effort, attempts, pool and budget ($1.5 per domain). One denylist fix before the run: `signal(` is refused unless it is
a member call or traffic's own `int signal(` accessor (the original traffic region contains it, and the offline `original` self-test was `denied` until this was fixed); the other self-test outcomes were re-checked afterward. Results are appended below whatever they are.
