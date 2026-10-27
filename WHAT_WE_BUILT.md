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

## Third-party dependencies
None so far (C++17 standard library only).
