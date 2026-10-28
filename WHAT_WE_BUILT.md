# What we built during the hackathon

Building period: Oct 27 to Nov 4, 2026. Everything in this repository was created in this fresh repo
during that period. No code was imported from earlier projects.

## Log
- 2026-10-27: repo scaffold, CMake build, MIT license, public-docs research notes (`docs/STEP0_API_NOTES.md`).
- 2026-10-27: deterministic 2D physics simulation with a controller-driven body (`src/sim`), seeded from a scenario description.
- 2026-10-27: rule checker (6 rules), 10 seeded bugs behind flags, hidden ground truth file for the harness only.
- 2026-10-27: tools (run_scenario, check_rules, inspect_state, render_frames, shrink, replay), PNG renderer, failure-preserving shrinker.
- 2026-10-27: random-testing baseline, evaluation harness, locked bug list and seeds (3 held-out bugs), unit tests.
- 2026-10-27: model interface with three backends (real Meta API client over Chat Completions via curl, JSONL replay cache, scripted mock),
  per-call token/cost/latency log and a hard USD budget (`src/model`, `configs/`).
- 2026-10-27: agent loop (native tool calling with a strict JSON-action fallback), citation validation that rejects reports citing
  nonexistent steps, bodies or values, and a vision cross-check step (`src/agent`). Tested end to end against the mock backend only.
- 2026-10-27: simulator v2 and bugs b11-b13 after controls found a weakness in v1; the web viewer (playback, evidence, scoreboard, live runs).
- 2026-10-28: the simulator seam (`SimFn`), unmodified Box2D through the same rules, orbit, cart-pole and traffic-signal domains, each with seeded bugs, locks, baselines and evaluations (cart-pole took three attempts; the history is kept).
- 2026-10-28: bring-your-own-simulator protocol, `rc_hunt`, `rc_repro`, `rc_diff`; agent v2 (checked counterfactual, frames in the loop); the ablation on fresh held-out bugs; scorer audit.
- 2026-10-28: triage evaluation (fuzzer finds, agent explains, against a no-model localizer); spec-to-rules; guided search tested offline and dropped; the single-file shareable report.
- 2026-10-28: API and MCP server, GitHub Action and CI (which found a Linux build error and a cross-platform determinism difference), sanitizer and fuzzing pass (16 undefined-behaviour sites fixed), exec-engine deadline, process-group kill and memory cap.
- 2026-10-28: REBOUND tested as an external engine, a 14-scenario literature corpus, and a reproducible hang in REBOUND's default IAS15 mode; a 160-mutant benchmark with hand-written, model-written and metamorphic oracles.
- 2026-10-28: the generic domain (a model card makes any simulator testable without C++), the `rc` front door, `scripts/reproduce.sh`, a run-to-run variance study, an adversarial evaluation, and the security, architecture and contributing documents.
- 2026-10-28: repair loop (sandboxed model-written code, bit-identity acceptance), invariant miner, validator red team, Boost.odeint engine, blind-bug-set mechanism, keyless local-model endpoints, Linux CI evaluation, multi-change mutants; full-set triage replication on two older models.

## Third-party dependencies
None in the core (C++17 standard library only). Box2D and REBOUND are used as external engines under test and are not included; see `THIRD_PARTY.md`.
