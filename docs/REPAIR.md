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
