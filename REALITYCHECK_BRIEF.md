# RealityCheck: build brief for Claude Code

## Goal
Build **RealityCheck**, an AI-driven adversarial tester for simulations. It takes a simulation plus a set of declared rules (invariants) and hunts for situations where the simulation breaks them. When it finds a failure it shrinks it to the smallest reproducible scenario, shows it visually, and explains the likely cause.

Entry for the Meta Global AI Developer Hackathon, Indie Developer track, problem statement **"Push the Frontier"** (software, robotics and physical systems: understand and control a physical system better). Solo entry unless I say otherwise.

## Step 0: before writing any code
1. Read the Meta Model API docs (overview, quickstart, model list, API cookbook) and report back: which Muse models exist, which support image input, which support tool/function calling and structured output, rate limits, pricing per token, and how I authenticate. Do not rely on memory for any of this.
2. Report which models are eligible for the "Best Build on Meta Open Weight Models" bonus (Muse Spark Open or Muse Glimmer via a partner such as Baseten; for the hackathon the Baseten model name is `meta-models/muse-spark-open-1026`). Tell me if our design can run on both a hosted Muse Spark and the open-weight model with a one-line config change.
3. Report the submission requirements as stated in the participant guide: public repo, a 3-minute demo video, everyone listed, deadline **Wed Nov 4, 9:00 PM Eastern**. Stop and ask me if anything conflicts with this plan.

## Rules that shape the build (from the participant guide)
- **New work only.** Everything shown must be built during the event (building period Oct 27 to Nov 4). Start a fresh public repo. Do not import code from any earlier project of mine. Keep a clean commit history from day one, and add a short `WHAT_WE_BUILT.md` that lists exactly what was created during the hackathon, so judges can identify it. Third-party libraries are fine if their licenses allow it and they are listed.
- **Meta models must be central.** Stage 1 of judging is pass/fail on whether the project "reasonably used" Meta's models. 25% of the score is "Use of Meta AI tooling: did they go beyond a basic integration?" A design where the model only writes an explanation at the end is not enough.
- **Judging weights:** Impact 30%, Use of Meta AI tooling 25%, Demo 25%, Technical depth 20%. The demo and the story matter as much as the engineering.
- Avoid the guide's "generic project" list (basic RAG apps, simple image analyzers, chatbots, and so on).

## The design: the model is the tester, the engine is the ground truth
Do not make the LLM guess pass/fail. The deterministic engine decides what is a violation. The model decides where to look.

1. **Simulation under test.** A small, deterministic 2D physics world (rigid bodies, walls, gravity, collisions) plus one controller-driven object (for example a simple vehicle or arm) so it is not only bouncing balls. Written in C++. It must be reproducible from a seed and a scenario description.
2. **Rules.** Declared as code or a small spec: no tunneling through walls, speed and energy bounds, objects stay inside the arena, no NaNs, controller limits respected. Checked at every step.
3. **Seeded bugs.** At least 8 injected bugs, each toggled by a flag, for example: collision check skipped above a speed threshold, wrong order of integration and collision resolution, floating-point blow-up at a very small time step, a corner case in wall-corner contact, a restitution bug above 1.0, a stacking instability. Ground truth (which bug is on) is recorded in a separate file that the tester and the model never see.
4. **Deterministic tools:** `run_scenario`, `check_rules`, `inspect_state(step)`, `render_frames(step range)`, `shrink(scenario)`, `replay(scenario)`.
5. **The agent (Muse Spark, through the Meta Model API).** It reads the simulation's description and the rules, forms hypotheses about where they might break ("a fast object near a thin wall corner may tunnel"), calls the tools to test each hypothesis, looks at the results and refines. It stops when it has a confirmed, shrunk failure or runs out of budget. Use tool calling if the model supports it; otherwise a strict JSON action protocol. Report which one.
6. **Vision step.** For each confirmed failure, render the frames around the violation and give them to the model (if it supports image input) to describe what went wrong visually and to cross-check the numeric explanation. The final report must be grounded in the exact steps and states it cites. Reject and retry explanations that cite states that do not exist.
7. **Output.** A failure report: the minimal scenario, a replayable trace, the violated rule, a frame strip, the model's explanation with cited steps, and whether the explanation matches the recorded state.

## The demo (3 minutes, the main artifact judges will watch)
A web viewer that replays recorded traces (the viewer can be JavaScript, everything else should be C++). Story:
1. Ordinary tests pass on the simulation.
2. RealityCheck's agent starts hunting. Show its hypotheses and tool calls streaming in real time.
3. It finds a failure: an object visibly tunnels through a wall.
4. It shrinks the scenario to a tiny reproduction and replays it side by side with the original.
5. The explanation appears with cited steps and the frame strip.
6. Show the scoreboard: bugs found by RealityCheck vs a plain random-testing baseline with the same time budget.
Make the live run work end to end with one command. Add a recorded fallback, clearly marked as recorded, in case an API call is slow.

## Evaluation (build the harness early)
- Baseline: random scenario generation with the same wall-clock and the same simulation-step budget, no model. Optionally a second baseline: a simple property-based tester with smart sampling.
- Metrics per seeded bug: found or not, number of scenarios until first violation, size of the shrunk scenario, whether the model's explanation names the true seeded cause, API cost and latency.
- Lock the list of seeded bugs and the test seeds in a commit before running the agent on them. Never tune on the locked set. Hold out at least 3 bugs the agent was never developed against.
- Report failures honestly: bugs the agent missed and explanations it got wrong. No unmeasured claims in the README or video; every number must come from a script in the repo.

## Constraints
- Prefer **C++** for the simulation, checker, shrinker and tool server. A thin HTTP client for the Meta API can be in whatever is least painful; tell me before choosing a non-C++ component and why. The web viewer is JS.
- CMake build, one command to build and one to run the demo.
- Secrets in environment variables only. Never commit API keys.
- Unit tests for the checker, the shrinker (it must always preserve the failure) and the citation validation.
- Open-source license file and an architecture diagram in the README.
- Cost control: log tokens and cost per run, and stop if a run exceeds a budget I set. My credits are limited ($150 Meta, possibly more from partners).

## Phases (about 8 days; deadline is Wed Nov 4, 9:00 PM Eastern, which is Thursday morning in Singapore, so aim to submit a day early)
1. Day 1: Step 0 report, repo, simulation, rules, one seeded bug, a "hello" call to the Meta API.
2. Days 2 to 3: all seeded bugs, tools, deterministic random baseline, shrinker, evaluation harness with locked bug list.
3. Days 4 to 5: the agent loop, vision step, citation validation, first evaluation run.
4. Day 6: the web viewer and the live demo flow.
5. Day 7: full evaluation, README, `WHAT_WE_BUILT.md`, demo script, video recording.
6. Day 8: buffer, clean-clone check, submission.
Stop and report at the end of each phase. If time runs short, cut scope (fewer bugs, no second baseline, drop the open-weight path) before cutting the evaluation or the demo.

## Definition of done
- One command builds and runs the live demo; the viewer shows the full story above.
- Results table (agent vs random baseline across seeded bugs) generated by a script.
- Public repo with README, architecture diagram, license, honest limitations and `WHAT_WE_BUILT.md`.
- A 3-minute demo video, and every submission requirement from Step 0 satisfied.
