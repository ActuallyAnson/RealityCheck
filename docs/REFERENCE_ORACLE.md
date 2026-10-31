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
- **It cannot see what a step does not expose.** The six cart-pole mutants it missed (lines 124, 127, 134, 171, 188 of `cartpole.cpp`) are in the controller's gain design (lines 124, 127, 134), the sensor-noise term (171) and the push window (188). The description does not specify the gains, the noise draw is not recoverable from the recording, steps overlapping a push are skipped by design, and the applied force is recorded only as a magnitude.
- **Orbit is close to transcription.** The orbit description states the force law explicitly, so the reference is easy; the 2 of 5 there is a small number.
- **Not attempted:** the rigid-body domain (contact dynamics: the description does not fix the substep rule or the positional correction, so a one-step prediction after an impact cannot be checked; free flight alone would test almost none of the mutants). The traffic domain is covered in the next section.
- **A change I made after seeing results, stated plainly:** my first judge compared each state component against its own step change. On the clean orbit simulator that flagged 18 of 300 scenarios (components that barely change make a relative tolerance meaningless). I changed the orbit judge to compare each body's position and velocity as 2-vectors, leaving F and K as they were. The clean result above is for the final judge; cart-pole was run only with the final judge.
- Orbit and cart-pole now have a second independent draw (`results/reforacle/*/draw2`; fresh model call, different code, 0 flagged steps on the clean simulator before scoring): it gives **identical counts** on the 40 sampled mutants and the planted set (orbit: 18 caught, 2 of the 5 silent; cart-pole: 25 caught, 13 of the 19 silent; planted: 1 of 2 and 3 of 5 silent; 0 false alarms), so the first draw was not a lucky one. (Traffic has two draws as well, below.) The planter of the held-out set is a model of the same family as the reference writer. The set is small (7 silent planted variants).

## Traffic (added 2026-10-29; pre-registered in `docs/PREREG_FINAL.md`)

Traffic is a discrete state machine with random arrivals, so the one-step physics reference does not apply. What changes:
- **The reference is a per-second controller.** It receives the recorded phase and timer (so one early disagreement cannot cascade), the queues after arrivals, the red-waits and the active events, and returns the next phase, the queues and the red-waits (`src/eval/ref_traffic_decl.hpp`). It may keep its own memory across seconds (pending pedestrian request, discharge credit).
- **Arrivals are not recorded**, so the harness tries every combination of "a vehicle arrived" on the approaches that can receive one and takes the combination whose prediction differs least from the recording; a second is flagged only if even that differs in any field (phase, four queues, four waits). No tolerance is involved: the comparison is exact.
- **The model needs a full written specification** (`src/eval/ref_traffic_spec.txt`), because the short description does not fix details such as the recall threshold or what an emergency does during a yellow. **This makes the traffic result a conformance check of the implementation against its specification, not a discovery of unspecified behavior.** The spec was validated first with a reference I wrote from the spec text (0 flags in 94,643 clean steps), so a failure of a model draw is the draw's, not the spec's.
- Pre-registered gate: 0 flagged steps on 1000 clean scenarios before any mutant is scored.

What happened, in order, nothing hidden:
1. **Draw 1 failed the gate** (49,583 of 466,898 clean steps flagged, 84 of 300 scenarios): the model read one sentence of my spec as allowing a phase to begin and end in the same second. `results/reforacle/traffic/draw1/` is kept. This is my spec's fault, not a property of the oracle, but it was found by running the model.
2. I reworded that sentence (**post hoc spec change**) and took two fresh draws that never saw draw 1. Both passed the gate (0 of 1,534,319 steps on seeds 910000+, and 0 of 1,559,391 on 1000 fresh scenarios, seeds 930000+).
3. On the mutant evaluation's own seeds (700000+) both draws flagged 3 of 300 clean scenarios, and so did the reference I wrote: seconds with emergency vehicles active in both directions at once, where the simulator lets the first-listed event decide and the spec says nothing. Those seconds are now **not judged** (**post hoc harness change**, stated in the spec as "not specified"); afterwards 0 flags on all clean sets. All mutant numbers below are with this change; the tolerances (there are none) and the scorer were not touched.
4. **Headline draw: draw 2**, declared before any mutant was scored; draw 3 is a replication.

| set | changed behavior | caught by the reference | of which a hand-written rule also catches | **silent to every other oracle** | **of those, caught by the reference** |
|---|---|---|---|---|---|
| traffic, 40 sampled mutants, draw 2 | 38 | 30 | 7 of 7 | 31 | **23** |
| traffic, 40 sampled mutants, draw 3 | 38 | 30 | 7 of 7 | 31 | **23** |
| traffic, 10 model-planted (held out), draw 2 | 9 | 8 | 0 of 0 | 9 | **8** |
| traffic, 10 model-planted (held out), draw 3 | 9 | 8 | 0 of 0 | 9 | **8** |

No unchanged mutant or planted variant was flagged. Both independent draws give identical mutant counts and miss the same 8 silent mutants:
- 4 are changes to the initial queue or wait values (`traffic.cpp` line 122): the judge starts each second from the recorded state, so the initial state is never predicted;
- m002 (line 244) stops the last queued vehicle from being discharged: an unrecorded arrival can explain the leftover vehicle, and the judge accepts any arrival pattern that explains the recording (the price of arrivals not being recorded);
- m007 (line 236) skips the first second of the run, which the judge cannot see because it starts from the recorded state;
- m011 (line 237) draws arrivals for a non-existent fifth approach, an arrival-process change that a deterministic reference cannot see;
- m037 (line 229) changes the value of an emergency flag that the reference does not output, so it is never compared.
The one planted variant not caught (p09) stops the run after a few hundred seconds in its first scenarios, so almost nothing is compared.

**Totals across the three covered domains** (`scripts/refcheck_score.sh`, `results/reforacle/summary.txt`): the reference catches **38 of the 55 sampled mutants that nothing else sees** (orbit 2 of 5, cart-pole 13 of 19, traffic 23 of 31), so the share of changed mutants caught by some oracle goes from 58 of 122 (48%) to **96 of 122 (79%)** (pre-registered success bar: 70%). On the model-planted held-out sets it catches 12 of the 16 silent variants (orbit 1 of 2, cart-pole 3 of 5, traffic 8 of 9). Rigid bodies are not covered, so 9 of the 122 changed mutants there stay silent, plus 8 in traffic, 6 in cart-pole and 3 in orbit.

**How much to believe it.** The 70% bar was met, but read the number correctly: (1) traffic's catch rate depends on a specification I wrote and made precise; an unspecified behavior cannot be checked this way, and a real project has to supply that specification. (2) The changed-mutant count includes some harmless numeric changes nobody has triaged, so 79% is "caught by some oracle", not "79% of real bugs". (3) Three of the four domains' references come from one or two draws of one model. (4) I wrote the spec after reading the simulator, so the specification is faithful to the code by construction; what the oracle tests is whether the *implementation of a given spec* deviates from it, which is the situation of a team with a written spec and a second implementation, not of a team with neither.

## Second blind set (planter: the open-weight model; declared in `docs/PREREG_FINAL.md` before running)

The stored reference programs (orbit and cart-pole draw 1, traffic draw 2; none regenerated) and the unchanged judge, on the 90 orbit, cart-pole and traffic variants of the second blind set (`docs/BLIND.md`, `results/blind2`); per-variant outputs `results/reforacle/blind2`, scores `results/reforacle/blind2/score.txt`. rigid2d has no reference and is not scored.

| domain | changed behavior | caught by the reference | silent to rules and mined invariants | of those, caught by the reference | false alarms |
|---|---|---|---|---|---|
| orbit | 17 | 9 | 5 | 1 | 0 |
| cart-pole | 22 | 8 | 16 | 7 | 0 |
| traffic | 18 | 16 | 15 | 13 | 0 |
| **three domains** | 57 | 33 | 36 | **21** | 0 |

The reference recovers 21 of the 36 variants no rule or mined invariant sees (58%; 69% on the author-sampled mutants, so a little lower here). Counting a hand-written rule or the reference, 42 of the 57 changed variants (74%) are caught, against 79% on the mutant benchmark. Limits: the traffic reference needs a precise written spec (the same caveat as before), model-written invariants and metamorphic relations were not run on this set, and the planter is a model of the same family as the tester, not a person.
