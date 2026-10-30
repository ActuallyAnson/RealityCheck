# Handoff brief

For anyone (a person or another AI) picking up RealityCheck. Read this first, then `README.md`, `docs/PREREG_FINAL.md`, `docs/REAL_PROJECTS.md`, `docs/BLIND.md`.

## What it is
RealityCheck is a C++ tool that finds bugs in simulators and physics engines by running scenarios and checking them against oracles (hand-written rules, mined invariants, model-written invariants, differential runs, and reference implementations). A model-driven agent chooses the scenarios. It is built for the Meta Global AI Developer Hackathon (Indie Developer track). Deadline: Wed Nov 4, 9 PM ET. Target submission: the evening of Nov 3 ET. Repo: https://github.com/ActuallyAnson/RealityCheck (public, `main`, MIT).

## Rules that must keep holding
- **Never commit secrets.** Keys live in `~/.zshrc` (`MODEL_API_KEY`, `BASETEN_API_KEY`). Never print them, paste them, or write them to a file in the repo. Scan tracked files before any submission.
- **No Co-Authored-By trailer** and no session link in commits. Stage paths explicitly. Commit small and often on `main`.
- **Core in C++.** Say so before using another language (viewer JS is the exception). The API client shells out to `curl`.
- **No hardcoded answers** and no ground truth in tester prompts.
- **Every number in README or docs must come from a script in the repo.** Declare experiments in `docs/PREREG_FINAL.md` (addenda) before running them. Report every result, including negatives. Never edit a result.
- **Demo hold:** no demo script, recording, shot list or video until the owner lifts the hold. UI work is fine.
- **Credits:** lifetime ledger `~/.realitycheck/spend_ledger.jsonl`, cap $100 (`rc spend`). About $50 is spent. Do not raise the cap without being told.
- Keep CPU low (`nice`, at most 2 workers, `build_dev/` for dev builds).

## Where things stand
Working and measured (details and caveats in README):
- UI with Scan and Oracle-gap screens.
- Triage localization: agent 53/88 vs baseline 37/88 (p=0.0195), replicated.
- Oracle coverage: 79% (96 of 122 changed mutants caught by some oracle), replicated over two reference draws.
- Agent v5 with boundary-value sampling beats random on five seeded hard bugs: 45/100 vs 17/300. The caveats are important: the bugs are mine, a model-free boundary sampler alone gets 62/200, and one bug (b12) carries most of the effect.
- CI is green; `scripts/reproduce.sh` regenerates every table.

## What is lacking (honest list)
1. **Real-engine fix discrimination does not work.** On REBOUND and Box2D historical fixes, rules fire equally on fixed and buggy builds, differential testing only says "changed", and a truth-oracle comparison on REBOUND TRACE was inconclusive (post closer 3 vs pre 7, p=0.34). Two of five fixes are never reached by my scenarios. The engines run fine; the limits are the oracle and scenario reachability. See `docs/REAL_PROJECTS.md`.
2. **No human-planted blind bug set.** Every bug is mine or a model's. Mechanism exists (`docs/BLIND.md`); it needs another person for about 30 minutes.
3. **rigid2d has no reference oracle**; its 9 silent mutants are mostly controller code the description does not specify. The traffic oracle depends on a spec written after reading the code.
4. **26 of 122 changed mutants are caught by nothing**; the harmless share is unknown. Bug c02 is found by nothing.
5. **Toy-sized targets:** four small simulators plus harness runs on REBOUND and Box2D.
6. **One model family** (Muse Spark), small samples, one run per seed, model-dependent evaluations not re-run on Linux.
7. **No demo or video** (on hold; submissions usually need one).
8. The README is long and buries the strongest results. No outside user has tried the tool.

## Suggested tasks, in priority order
1. Plant a human blind set following `docs/BLIND.md`, lock it, evaluate, publish.
2. For REBOUND/Box2D: pick fixes whose changed code the scenarios provably reach (instrument with coverage first), and build an oracle from the engine's documentation or a high-precision reference, pre-registered before running. A negative result is acceptable and should be reported as such.
3. README claim audit: tie every number to a script; add the randbase, reforacle, headroom and extreme tables to `scripts/gen_results.sh`.
4. Shorten the README top so the three strongest, fully caveated results come first.
5. Final checks: CI green on the newest commit, secret scan, optional `v1.0.0` tag.

## Getting oriented
- Build: `scripts/reproduce.sh build_dev` (uses `build_dev/`). Tests: 798 checks, 0 failed.
- Real-project scripts: `scripts/build_rebound_hist.sh`, `scripts/build_box2d_hist.sh`, `scripts/rebound_history.sh`, `scripts/box2d_history.sh`. The third-party clones are built outside the repo and nothing from them is copied in.
- Results live in `results/`; protocol in `docs/PROTOCOL.md`.
