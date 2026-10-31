# rc_scan: how to read a report, and how to fix an adapter

`docs/SCAN.md` says what the tool does and how well it did on real engines. This page is for the person holding a report.

## Reading a finding
Each finding is one row: the rule that broke, how many of the scenarios broke it, and the smallest scenario that still does. Then, per finding, a "depends on" line: every parameter was tried at its range ends, its default and every choice, and the line lists the values at which the finding stays and where it disappears. Read it first. Three outcomes:
- **A narrow dependence** ("only when mode = proportional and gain = 8 and capacity is 57 or 60"): a candidate bug. Reproduce it by hand with just those values.
- **A dependence on an extreme** ("only at dt = 0.05"): usually a limit of the method, not a bug. Check the library's manual for the allowed range.
- **No single parameter matters**, or the rule is about a start value or a scale: usually the learned rule is too tight. Ignore it or add a `--require` rule of your own.

Rule kinds you will see: a mined bound (`signal <= v`), a drift rule (`stays within x of its start`), a step rule (`changes by at most x per step`), a constant rule (it never changed in normal runs), a scaled rule (it follows a parameter), and `--require` rules you wrote. A mined rule is a guess from the program's own behavior, so "it broke" means "unlike what we saw", not "wrong".

`run failed` with "did not answer within N s" is a hang; `non-finite signal` is a NaN or infinity; `nondeterministic` means the same request gave different answers.

## What a clean scan means
Little. A bug that is present in every run looks normal. State what you know with `--require "signal<=value"` (for instance a count of objects that left the arena `<=0`), widen the parameter ranges, and add a start family your program might be sensitive to (symmetric, equal values, zero): the IAS15 hang was only found once the scenarios could contain a symmetric start (`docs/SCAN.md`).

## Adapter problems that happened to real people (agents) and what fixed them
- Nothing works, or 90% of runs fail: the library prints warnings on stdout. The protocol needs exactly one JSON line per request on stdout and nothing else; send library output to stderr or `/dev/null`.
- "choices needs at least 1 string": you gave `--choice name:` with no values.
- The first request hangs: the program hangs on its default parameters; the scan stops with a message after `RC_EXEC_TIMEOUT_S` seconds (default 60). That is a finding about your program.
- A signal has the wrong length: every signal needs `steps + 1` numbers (step 0 plus one per step).
- Every finding looks like an escaped object or a huge value: check the adapter first; two of the agents' findings were a wrong mass/inertia setup and an "outside" test that counted transient penetration.
- The report only lists parameters that differ from the default; the "full parameters" list under the table shows all of them, and the default can be what triggers a finding.

## Options that change what is learned
`--absolute-bounds` turns off the refinements (relative-to-start, scaled and constant rules) and keeps plain mined bounds. `--require "signal<=value"` / `">="` adds your own rule. `--scenarios`, `--steps` and `--out` do what they say; `RC_EXEC_TIMEOUT_S` sets the per-request time limit.
