# Generic domain: test a simulator without writing a C++ domain

The built-in domains each needed a scenario schema, rules, prompt text and a renderer in C++ (`docs/ADD_A_DOMAIN.md`). The generic domain removes that for any program that can emit
named numbers once per step: you write a JSON **model card**, point `RC_MODEL_CARD` at it, and the same tools, shrinker, agent, API and spec-to-rules run on your program.

## The model card

```json
{
  "name": "water tank level controller",
  "description": "plain-language behaviour of the simulator (the model reads this)",
  "command": "./build/generic_tank",
  "parameters": [
    {"name": "inflow", "type": "float", "min": 0, "max": 8, "default": 2, "text": "mean inflow per step"},
    {"name": "capacity", "type": "int", "min": 5, "max": 60, "default": 20},
    {"name": "gain", "type": "float", "min": 0.1, "max": 8, "default": 1, "scale": "log"},
    {"name": "mode", "type": "choice", "choices": ["proportional", "bang_bang"], "default": "proportional"}
  ],
  "steps": {"min": 20, "max": 400, "default": 200},
  "signals": [{"name": "level", "text": "water level"}, {"name": "capacity", "text": "constant"}],
  "rules": [
    {"id": "level_within_capacity", "text": "The level never exceeds capacity.", "kind": "bound", "signal": "level", "op": "<=", "rhs_signal": "capacity"}
  ]
}
```

Rules use the invariant language of `docs/SPEC_RULES.md` (`bound`, `conserved`, `monotone`, `exclusive`, `min_run`, `max_run`), over the signals you list; `no_nan` is implicit. A card with an unknown
signal in a rule, a bad range or a duplicate name is rejected with a precise message. Parameter types: `float` (optionally `"scale":"log"`), `int`, `choice`.

## The protocol

Your program is started once (`"command"` in the card, or `--engine 'exec:CMD'`) and reads one JSON line per request:

    {"op":"simulate","scenario":{"domain":"generic","seed":1,"steps":200,"params":{"inflow":2.0,"capacity":20,"gain":1.0,"mode":"proportional"}}}

and answers with one line: `{"ok":true,"signals":{"level":[...], "capacity":[...]}}`, each array holding `steps + 1` numbers (step 0 is the initial state; `null` for a non-finite value), or
`{"ok":false,"error":"..."}`. Choice parameters arrive as their string. The program has the exec engine's deadline (`RC_EXEC_TIMEOUT_S`) and memory cap (`docs/PROTOCOL.md`); `exec:` is refused by the API unless the operator allows it.

## What works

`rc_hunt` (random search and regression bundles), `rc_agent` (the model-driven tester, with citations that name your signals), `rc_repro`, `rc_spec` (the model writes invariants from your description),
the shrinker (each parameter toward its default, then halfway, then rounded; the run shortened to the failure), `inspect_state` (all signals at a step), and the API/MCP server
(no built-in simulator exists for the generic domain, so API calls need an engine). Not available: `render_frames` (there is nothing to draw), the viewer, seeded-bug evaluation, rule margins.

## Worked example: `examples/generic/`

A water-tank controller written as a separate C++ program with one **deliberate flaw** of mine: in proportional mode with gain above 4 and capacity at least 40 the spill clamp is skipped. Results, all on this planted flaw:

| method | result |
|---|---|
| random hunt (`rc_hunt`, 10 seeds, 1500 scenarios each) | found in **10 of 10** seeds; first violation after 53 to 465 scenarios; shrinks to 3 to 5 changed parameters |
| agent v2, no vision (5 runs, caps $0.30 and $0.12) | found and reported in **2 of 5** ($0.045 and $0.026); 3 runs ended at their budget cap without finding it ($0.28, $0.10, $0.10) |
| spec to rules (`rc_spec`, one run, $0.015) | the model proposed **`level <= capacity`** from the card's description; see below |

The spec-to-rules result needs care. The engine validates proposed invariants on the program's own runs, and for an unknown program there is no separate clean reference. Here the correct invariant
`level <= capacity` was **rejected on "clean" runs**, with a counterexample at seed 69, because the program itself violates it. The tool now keeps every rejected invariant with its counterexample scenario
(`results/generic/spec/spec.json`, `counterexample_scenario`) so it can be shrunk and triaged: for a program with no clean reference, "rejected on clean runs" means **either a wrong invariant or a bug**, and only a person (or an
agent plus a person) can tell which. The model also proposed seven true but vacuous bounds (valve in [0, 1], capacity in [5, 60], and so on).

So on this target random testing is at least as good as the agent at finding the flaw, as in the other domains (`docs/MUTATION.md`). The agent's contribution is the grounded report: it names the signal values at the
first violating step and runs a one-change counterfactual (in the accepted run: lowering the inflow removes the failure, which is a true but not the root-cause change, since the trigger is gain and capacity).
The first run needed a fallback field name for its citations because the citation checker did not yet know generic signal names; that was fixed (a citation now names a signal of the card) and the first run's files were not kept.

## Limits

- Only as good as the card's rules: the examples above and `docs/MUTATION.md` say how much a model-written rule set catches.
- Parameters are a flat bag of numbers and choices; no lists of structures (a variable number of bodies or events needs a built-in domain).
- The simulator is trusted to be deterministic for a given scenario; the tool does not check this unless the metamorphic `repeat` relation is added to the card's checks.
