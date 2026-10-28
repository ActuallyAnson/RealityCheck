# A model-written reference as a test oracle

**The problem.** The mutation benchmark found that 64 of 122 behavior-changing bugs violate no declared rule, no model-written invariant and no metamorphic relation, and `docs/MINING.md` showed that learning invariants from the system under test cannot close that gap. Closing it needs information from outside the system.
**The idea.** Give a model only the plain-language description of the simulator (no source, no rules, no bug information) and ask it for an independent implementation of one time step. A program, not the model, then compares that reference with what the system under test recorded.

## Method (`rc_refgen`, `rc_stepdump`, `refrun`, `rc_refjudge`, `scripts/refcheck_worker.sh`)

1. `rc_refgen` shows the model the description with the declared-rules section removed, plus the C++ interface of one step (`src/eval/ref_*_decl.hpp`), and takes the function body it writes. Code containing I/O, preprocessor lines or system calls is refused; the model may revise only after a **compile error** (it never sees a counterexample from the system under test, so the reference stays independent of it). One draw per domain, `muse-spark-1.3`, about $0.016 each.
2. `rc_stepdump` is linked into the build under test and records, at up to 25 sampled steps of each of 300 random scenarios, the state before a step and the state after it.
3. The model-written reference runs **sandboxed** (no network, no file writes, no forking) on the "before" state only and predicts the "after" state, once with a step of `dt` and once as two steps of `dt/2`.
4. `rc_refjudge` flags a step when the recording differs from the prediction by more than `max(F x |recorded change over the step|, K x |one step - two half steps|, 1e-9)`. **F = 0.02 and K = 10 were fixed before any run and are not fitted to the clean simulator**; the second term is the reference's own error estimate. One-step comparison avoids the chaos that makes whole-trace differential testing report noise on contact dynamics.
   Cart-pole: the applied force is recorded only as a magnitude, so the judge uses the sign that fits better; steps overlapping an external push are skipped.

## Results (one reference per domain, 300 scenarios, seeds 700000+)

Clean simulator: **0 flagged steps** in 7803 (orbit, 300 scenarios) and 26021 (orbit, 1000 fresh scenarios, seeds 800000+); 0 in 10230 and 10239 (cart-pole, two seed sets). The worst error was 0.33 of the tolerance for orbit and 1.8e-5 for cart-pole. No unchanged mutant was flagged in either domain.

| set | changed behavior | caught by the reference | of which a hand-written rule also catches | **silent to every other oracle** | **of those, caught by the reference** |
|---|---|---|---|---|---|
| orbit, 40 sampled mutants | 25 | 18 | 16 of 20 | 5 | **2** |
| cart-pole, 40 sampled mutants | 35 | 25 | 11 of 15 | 19 | **13** |
| orbit, 10 model-planted (held out) | 9 | 4 | 3 of 7 | 2 | 1 |
| cart-pole, 9 model-planted (held out) | 7 | 3 | 0 of 2 | 5 | 3 |

(Plus 6 orbit mutants that hung or crashed in the original evaluation; all 6 are flagged by the reference.) Raw per-scenario rows: `results/reforacle/*/`; scored by `scripts/refcheck_score.sh` into `results/reforacle/summary.txt`.
**Across the two domains the reference catches 15 of the 24 mutants that nothing else sees, which moves the share of changed mutants caught by some oracle from 58 of 122 (48%) to 73 of 122 (60%).** On the held-out planted set it catches 4 of the 7 silent variants.

## What this does and does not show

- **It works where the physics is specified and a step can be checked from the recorded state.** The cart-pole description gives no equations; the model derived the plant dynamics itself, with the sign conventions I would have chosen, and agreed with the clean simulator to 1e-5 of the tolerance. Tightening F to 0.001 after the fact (a sensitivity check, not a result) catches the same 25, so the misses are not a tolerance problem.
- **It cannot see what a step does not expose.** The six cart-pole mutants it missed (lines 124, 127, 134, 171, 188 of `cartpole.cpp`) are in the controller gains and bookkeeping, which the description leaves unspecified; the force actually applied is recorded only as a magnitude.
- **Orbit is close to transcription.** The orbit description states the force law explicitly, so the reference is easy; the 2 of 5 there is a small number.
- **Not attempted:** the rigid-body domain (contact dynamics; one-step prediction after an impact needs the collision model) and the traffic domain (seeded random arrivals; it needs a different kind of reference). 24 of the 64 silent mutants are in the two domains covered; the other 40 are untouched.
- **A change I made after seeing results, stated plainly:** my first judge compared each state component against its own step change. On the clean orbit simulator that flagged 18 of 300 scenarios (components that barely change make a relative tolerance meaningless). I changed the orbit judge to compare each body's position and velocity as 2-vectors, leaving F and K as they were. The clean result above is for the final judge; cart-pole was run only with the final judge.
- Both references come from one model draw each; I did not test how much a second draw would differ. The planter of the held-out set is a model of the same family as the reference writer. The set is small (7 silent planted variants).
