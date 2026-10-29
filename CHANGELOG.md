# Changelog

## 1.0.0 (Meta Global AI Developer Hackathon submission build)

Added during the hackathon, in rough order:
- Deterministic tools, rule checker, shrinker, tester agent v1 (frozen) and v2 (checked counterfactual, frames in the loop) on a rigid-body simulator with 13 seeded bugs; locked evaluation, random baseline, controls.
- Orbit, cart-pole and traffic-signal domains; the viewer; the shareable single-file report.
- Bring-your-own-simulator protocol, `rc_hunt`, `rc_repro`, `rc_diff`; unmodified Box2D and REBOUND as external engines; an external scenario corpus.
- Triage mode and evaluation (agent 52/83 against 35/83 for a no-model localizer); spec-to-rules; guided search tested offline (negative); mutation benchmark with metamorphic oracles.
- API and MCP server, GitHub Action, CI on Linux and macOS, sanitizer and fuzzing pass (16 undefined-behaviour sites fixed), exec-engine deadline, process-group kill and memory cap.
- The generic domain (model card), the `rc` front door, security and architecture documents.
- Repair loop with sandboxed model-written code and bit-identity acceptance (96 of 105 restored, 9 rules-pass-but-wrong); invariant miner (negative result on the mutation benchmark; precise screen on outside engines); validator red team; Boost.odeint as a third outside engine; full-set triage replication on two older models; blind-bug-set mechanism; keyless local-model endpoints; Linux CI evaluation; multi-change mutants; pinned rigid-body mutation base.; opt-in strict report validator (`rc_strictcheck`, `scripts/strictcheck.sh`) measured on all 244 recorded triage reports.; model-planted bug set (`rc_plant`, 39 variants locked before evaluation).; hybrid agent v3/v4 with a `sample_random` tool (traffic 15/15), `rc_spec --mined`.; model-written reference oracle (`rc_refgen`, `rc_refjudge`): 15 of 24 silent mutants caught in orbit and cart-pole.
- Final week (pre-registered in `docs/PREREG_FINAL.md`): reference oracle extended to traffic (two independent draws, 23 of 31 silent mutants and 8 of 9 held-out planted variants caught; any-oracle coverage 48% to 79%; one draw failed its clean gate and the spec was reworded, disclosed); exact random-testing baseline (`rc_randbase`) showing the sampled mutants have almost no search headroom; stronger no-model triage baseline (agent still wins, p = 0.0008); viewer Scan and Oracle gap screens.

- Agent v5 (boundary-value option in `sample_random`): pre-registered test on the five hard bugs, 45/100 against 17/300 for random testing (p = 3e-18; p = 3e-7 without b12). Model-free boundary sampler: 62/200 on the same bugs.
