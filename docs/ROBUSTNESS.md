# Robustness: testing the tester

A verification tool that crashes or misbehaves on bad input is not trustworthy, so the input-facing code is tested under sanitizers and fuzzed.

## What is run

- **Unit and end-to-end tests under AddressSanitizer and UBSan** (`rc_tests`, 771 checks): 0 findings.
- **`rc_fuzz`**, a mutation fuzzer (`src/eval/fuzz_main.cpp`; Apple's clang has no libFuzzer, so it is a small loop of my own). Seeds are valid scenarios of all four domains and a valid
  invariant list. Mutations: byte flips, deletions, duplicated runs, truncation, swaps, and replacing numbers with hostile values (`1e999`, `NaN`, `-1`, 2^32, huge integers, `null`, nested arrays).
  Targets: the JSON parser (parse and re-serialize), the scenario parsers and validators for every domain (and, when a mutated scenario is valid and small, the simulator, the rule checker
  and the signal extractor), the invariant parser, and the API operations (`run`, `shrink`, `bundle`, `hunt`, `random_scenario`, `domains`). A crash, sanitizer report, hang (60 s watchdog) or
  an API result without `ok` is a finding. Build: `cmake -S . -B build_san -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -O1"`,
  then `./build_san/rc_fuzz --iterations 400000 --api-iterations 1500 --seed N`. CI runs a shorter pass on every push.

## What it found

The first run found **undefined behaviour in 16 places**: converting `inf`, NaN or out-of-range numbers read from JSON to `int` or `uint64_t` in the scenario parsers (3 in the rigid-body and common parser,
9 in traffic, 2 in orbit, 2 in cart-pole). In practice the validators would have rejected such scenarios, but the cast itself is undefined before they run. Fix: saturating `clamp_int` and `clamp_u64`
(`src/sim/scenario.hpp`) for every integer read from untrusted JSON; values in range convert exactly as before (the pinned rigid-body hashes and all tests are unchanged).

After the fix: 4 seeds x 400000 parser iterations and 1500 API calls each in log-and-continue mode (no report in the output), then 2 more seeds with UBSan set to abort on any finding, plus the full test suite under both: no crash, no hang, no sanitizer report.
(The first version of the fuzzer printed "no sanitizer report" even though UBSan had logged findings, because UBSan continues by default; the final-message claim is now backed by `-fno-sanitize-recover`.)

## What it does not show

- Leak detection is off (`detect_leaks=0`; it is not supported on this machine's ASan).
- Coverage is by volume, not guided by coverage feedback, so deep paths behind several valid fields are reached mostly through the seeded valid scenarios.
- The model client, the exec-engine child handling and the HTTP request framing are exercised by tests and by hand, not by this fuzzer.
- Only the paths on the macOS build were fuzzed locally; the Linux sanitizer run is in CI.

## Linux re-run of the model-free evaluations

CI job `linux-evaluation` (Ubuntu) rebuilds and re-runs, with no model: the four random-baseline evaluations (orbit, cart-pole, traffic, and the rigid-body ablation lock), the validator red team, and the external-scenario corpus. Result (run 40710656165): **all four random-baseline tables are byte-identical to the committed macOS tables**,
the red team gives the same table, and the corpus runs (Box2D and REBOUND rows are "n/a" there because those engines are not installed on the runner). So the libm difference that changes pinned trace hashes did not change any bug-finding count in these evaluations. What has still not been re-run on Linux: the model-dependent evaluations
(agent, triage, spec), which are API runs that cost money and whose replayed tables depend on the simulator version, not the platform alone.

### Recorded model reports replayed on Linux (no new model calls)

The 244 triage reports written by the model on macOS (three models, 83 failures each) were re-validated on Linux (Docker, Ubuntu 24.04, `rc_strictcheck`): each session's shrunk scenario and counterfactual were re-simulated on the Linux build, then the report was checked with the default and the strict validator.
Result: **the default validator accepts 244 of 244 and the strict validator accepts the same 47, 42 and 37 reports as on macOS; every forgery count is identical** (`results/validator/strict.txt` is the macOS output; the Linux output was compared line by line).
This is a replay of macOS-recorded model output, not new model runs on Linux, so it shows that the engine's judgments of those reports do not depend on the platform; it does not show that the model would behave the same on a Linux run (it does not see the platform).
CI repeats this on every push (`linux-evaluation`, step "Recorded reports on Linux").

