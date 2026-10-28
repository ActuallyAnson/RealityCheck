# Architecture

## Layers

```
 rc (front door) ── rc_hunt rc_agent rc_triage rc_spec rc_repro rc_diff rc_eval rc_api_server rc_demo ...
        │
   tester agent (src/agent)  ◄── model client (src/model: Meta API via curl, replay/record/mock backends, budgets, ledger)
        │ tool calls
   deterministic tools (src/tools: run_scenario, check_rules, inspect_state, render_frames, shrink, replay)
        │                       ▲
   shrinker (src/shrink)        │ same interface, any engine
        │                       │
   rule checker (src/rules) ◄── SimFn: Scenario -> Trace ──► built-in simulators (src/sim: rigid2d, orbit, cartpole, traffic)
                                                            exec engines (src/external: any program over JSON lines; Box2D; REBOUND adapter)
                                                            generic domain (src/sim/generic: a model card + an exec engine)
```

Everything below the agent is deterministic and model-free; the same functions are served over HTTP and MCP (`src/api`) and used by the evaluation harness (`src/eval`).

## Decisions and why

1. **The model never decides pass or fail.** Verdicts come from a rule checker that is a pure function of the recorded trace. The model chooses where to look and writes explanations, and a report is accepted only when
   every cited step and value matches the recording and (v2) a one-change counterfactual really passes. This is the design's anchor; everything measured in `docs/` is about where it helps and where it does not.
2. **A simulator is a pure function `Scenario -> Trace` (`SimFn`).** That one seam lets the built-in simulators, a persistent child process, Box2D and a mutated build be tested by the same tools, and makes replay, shrinking and caching trivial.
3. **Seeded bugs hide behind a mask only the simulator reads; the answer key is harness-only.** Tests check the prompts for leaks; the tester's tool results never name a bug.
4. **Replay caches key on the exact request bytes**, so a frozen agent version (v1) must keep byte-identical prompts and schemas; tests pin them (`rigid2d pins`). New behaviour goes behind a version or a new domain, never into an old prompt.
5. **Cost is capped on three levels** (per run, per evaluation, a lifetime ledger outside the repo) and enforced before a call is made.
6. **Evaluation is locked before it is run**: lock files in `data/` fix seeds, caps and bug lists; controls on a clean simulator accompany every evaluation; negative results and second attempts are reported as such.
7. **No third-party code in the core; the model client shells out to curl.** The build needs a C++17 compiler and CMake. This costs some convenience (a hand-written JSON parser, an HTTP server on sockets) and buys auditability and a build that works anywhere.
8. **Determinism is per platform.** Traces are bit-reproducible on one machine type; libm differences change them across macOS and Linux, so pinned hashes are checked on macOS only.

## Non-goals

- Not a physics engine, a simulator framework or a general fuzzer. It tests simulators others write.
- Not a source of ground truth about real-world correctness: it checks a simulator against *declared* rules (and, with a reference, against another engine).
- Not hosted. The API and viewer bind to loopback; there is no multi-user service, no auth beyond an optional token, and no sandbox for untrusted simulators.
- No claim that a model finds bugs better than random testing: `docs/` shows it does not, and shows what it does do (triage, grounded reports, writing some rules).
