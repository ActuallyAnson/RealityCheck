# Security and threat model

RealityCheck runs other people's simulators, parses untrusted JSON, calls a paid model API, and can serve a local HTTP API. This is what it defends, how, and what it does not.

## Assets
1. The model API key (`MODEL_API_KEY`) and the money behind it.
2. The machine running the tools.
3. The integrity of the verdicts: a "violation" or an accepted report must mean what it says.

## Threats and controls

| Threat | Control | Where |
|---|---|---|
| Key leaks through process listings, logs, transcripts or git | the key is read from the environment and written only to a 0600 curl config file in `/tmp` (never into `argv`); it is never logged; `.gitignore` excludes key files; the full git history was scanned for the key value (0 matches) | `src/model/model.cpp`, `.gitignore` |
| Runaway spend | three caps: per run, per evaluation, and a lifetime ledger outside the repo (`~/.realitycheck/spend_ledger.jsonl`, default cap $100); calls that could pass a cap are refused before they are made | `BudgetedModel`, `docs/STEP0_API_NOTES.md` |
| A hostile or buggy simulator (`exec:` engine) | it runs as the user, so `exec:` is **refused by the HTTP API and MCP server unless the operator passes `--allow-exec`**; every request has a deadline (`RC_EXEC_TIMEOUT_S`, default 60 s) after which the child and its whole process group are killed and the run stops with an error, never a finding; the child gets a best-effort memory cap (`RC_EXEC_MEM_MB`, default 4096, via `RLIMIT_AS`: enforced on Linux, only partly on macOS, and not verified here); a malformed or short reply is an error | `src/external/exec_sim.cpp`, `src/api/api.cpp` |
| Malformed or adversarial input to the parsers and API | the JSON parser, scenario parsers and validators, invariant parser and API operations are fuzzed under AddressSanitizer and UBSan (`rc_fuzz`, `docs/ROBUSTNESS.md`); out-of-range numbers are clamped before integer conversion; request bodies are capped at 1 MB; scenarios are validated against the public limits before any simulation | `src/eval/fuzz_main.cpp`, `src/sim/scenario.hpp` |
| Network exposure of the API or viewer | both bind to 127.0.0.1 only; the API has an optional bearer token; the viewer's live-run endpoint rejects cross-origin requests and needs an `X-RC` header | `src/api/api_main.cpp`, `src/demo/demo_main.cpp` |
| Path traversal through the viewer | static files are served from one root; `..` and backslashes are refused | `src/demo/demo_main.cpp` |
| Command injection in the model client | the only shell command built contains the curl binary, a numeric timeout and temp-file paths created by `mkstemp`; no scenario, model or user text is interpolated into it | `src/model/model.cpp` |
| The model being steered (prompt injection through scenarios, tool results or a simulator's text) or simply wrong | the model never decides pass or fail: verdicts come from the deterministic checker; a report is accepted only if every cited step and value matches the recording and, in v2, a one-change counterfactual really passes; the tester never sees seeded-bug information (tests check the prompts) | `src/agent/agent.cpp`, `tests/test_main.cpp` |
| Model-written code (repair loop `rc_repair`) | a denylist scan before compiling (preprocessor, process, file, network, dynamic-loading and assembly tokens), then the build runs under `sandbox-exec` with no network, no file writes and no forking, a CPU limit and a wall-clock timeout; offline self-tests show a patch that tries `std::system` is refused and creates no file | `docs/REPAIR.md`, `scripts/repair_sandbox.sb` |

## What is not defended

- An `exec:` simulator, once allowed, can do anything the user can. There is no sandbox (only the deadline, the process-group kill and the best-effort memory cap). Run only simulators you trust, or in a container.
- The model sees scenario and telemetry text; I have not measured how often a crafted simulator output changes what the model writes. The design limits the damage to a wrong explanation that the validator and counterfactual may reject, not a wrong verdict, but that is a design argument, not a measurement.
- The local API has no rate limiting and is not meant to be exposed beyond the machine.
- `/tmp` is used for the short-lived curl config and request body (0600); on a shared multi-user machine that is a weaker place than a private directory.
- Determinism differs between platforms (libm); a verdict on a borderline scenario could differ between macOS and Linux.

- The repair loop's denylist is a token scan, not a proof, and its sandbox is macOS-only (`sandbox-exec` is deprecated); the compiler itself is not sandboxed. Do not run `rc_repair` on a machine holding secrets you cannot lose.
- A `--build-cmd` or `--test-cmd` style repair of an arbitrary project is deliberately not offered: a project build runs arbitrary code, which needs a stronger sandbox than this one.

## Reporting

Open an issue, or email the address on the author's GitHub profile for anything that involves a key or a remote-code path.
