# Prompt for a blind bug planter (a different AI, or a person)

Give this to an AI that has NOT seen this repository's oracles, rules, results or tests. Give it only the four simulator files. Then follow `docs/BLIND.md` (lock by hash, evaluate, reveal).

```
You are helping test a bug-finding tool. I will give you the source of a small C++ simulator.
Write 10 DIFFERENT variants of it, each containing one realistic bug of the kind a careful
programmer could plausibly introduce by mistake. Save each variant as its own complete .cpp file
(named bug01.cpp ... bug10.cpp) in a directory I name, keeping the file compiling with the same
interface as the original.

Rules:
- One bug per file. Do not change the file's public interface or add preprocessor lines or system calls.
- Make them realistic and varied: wrong sign, off-by-one in a loop or index, a missing term in a
  formula, a wrong unit or constant, an update done in the wrong order, a missing clamp or
  boundary check, a comparison like < vs <=, a state that is not reset, an integrator that mixes
  old and new values, a rare special case handled wrongly (for example only when two things
  coincide, or only for very large or very small inputs).
- Include a spread: some bugs that change results a lot, some that only matter in rare or extreme
  conditions, and some subtle enough that a simple check ("is energy conserved?") would not notice.
- Do NOT make bugs that crash, hang, or only change printing or serialization.
- Do not tell me which lines you changed. Write one line per file describing the bug in plain words
  into a separate file called DESCRIPTIONS.txt, and do not show it to anyone until I ask.
- Do not look for or use any tests, rules or checkers; none exist for you.

Work only from the simulator source I give you.
```

Notes for the person running it:
- Give the planter `src/sim/orbit.cpp`, `src/sim/cartpole.cpp`, `src/sim/traffic.cpp`, or `data/mutation_base/rigid2d_world.cpp`, one domain at a time, and nothing else from the repository.
- Keep `DESCRIPTIONS.txt` out of the evaluator's view until after the results.
- Then: `scripts/blind_lock.sh DIR DOMAIN`, commit and push the manifest, `scripts/blind_eval.sh DIR DOMAIN`, and publish the descriptions next to the results.
