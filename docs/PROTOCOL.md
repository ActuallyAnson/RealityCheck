# Bring your own simulator

Any program, in any language, can be tested if it speaks this protocol on stdin/stdout (JSON lines, one request line
in, one response line out). RealityCheck owns the scenario space, the rules, the shrinker and the agent; your program
only has to simulate.

```
rc_hunt  --engine 'exec:./my_sim' --domain orbit --scenarios 2000 --bundles out/   # random hunt + regression bundles
rc_agent --live --engine 'exec:./my_sim' --domain orbit --out runs/x               # model-driven hunt
rc_diff  --a builtin --b 'exec:./my_sim' --domain orbit --tol 0.1 --horizon 1      # differential testing
rc_repro check --repro out/orbit_energy_conserved --engine 'exec:./my_sim'         # replay a saved failure
```

## Request

`{"op":"simulate","scenario":{...}}` where `scenario` is the domain's scenario JSON (the same JSON the agent's
`run_scenario` tool accepts; see the simulation description printed by `rc_tool --describe`). It always carries
`"domain"`, `"dt"` and `"steps"`.

## Response

`{"ok":true,"frames":[[...],[...]]}` with exactly `steps + 1` frames (frame 0 is the initial state). Each frame holds
four numbers per body, in scenario order: `x, y, vx, vy`. Use `null` for a non-finite value. On failure return
`{"ok":false,"error":"..."}`. RealityCheck derives energy and momentum itself from the frames.

| domain | bodies in a frame | extra |
|---|---|---|
| `rigid2d` | one `[x,y,vx,vy]` per body (controller scenarios are not supported by external engines) | |
| `orbit` | one `[x,y,vx,vy]` per body | |
| `cartpole` | one body: `[x, theta, v, omega]` | optional `"force":[...]` (controller force per step) and `"work":[...]` |

The program is started once and kept running, so per-scenario cost is the simulation itself. If it exits, writes a
malformed line, or does not answer within `RC_EXEC_TIMEOUT_S` seconds (default 60; it is killed), the tool stops with an error instead of reporting a false finding.

## Example

`examples/external/naive_orbit.cpp` is a small independent orbit integrator (fixed-step velocity Verlet) that shares
no code with the built-in simulator. With a 0.004 s substep it agrees with the built-in one; with 0.03 s,
`rc_hunt` reports energy-conservation failures and shrinks them to two bodies.

## What the protocol does not do

Domains are fixed (rigid2d, orbit, cartpole): your simulator must implement one of those scenario schemas. Testing a
different kind of system needs a new domain (a scenario schema, rules and a renderer); the spec-to-rules feature on the
roadmap is meant to lower that cost.
