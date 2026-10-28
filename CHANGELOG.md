# Changelog

## 1.0.0 (Meta Global AI Developer Hackathon submission build)

Added during the hackathon, in rough order:
- Deterministic tools, rule checker, shrinker, tester agent v1 (frozen) and v2 (checked counterfactual, frames in the loop) on a rigid-body simulator with 13 seeded bugs; locked evaluation, random baseline, controls.
- Orbit, cart-pole and traffic-signal domains; the viewer; the shareable single-file report.
- Bring-your-own-simulator protocol, `rc_hunt`, `rc_repro`, `rc_diff`; unmodified Box2D and REBOUND as external engines; an external scenario corpus.
- Triage mode and evaluation (agent 52/83 against 35/83 for a no-model localizer); spec-to-rules; guided search tested offline (negative); mutation benchmark with metamorphic oracles.
- API and MCP server, GitHub Action, CI on Linux and macOS, sanitizer and fuzzing pass (16 undefined-behaviour sites fixed), exec-engine deadline, process-group kill and memory cap.
- The generic domain (model card), the `rc` front door, security and architecture documents.
