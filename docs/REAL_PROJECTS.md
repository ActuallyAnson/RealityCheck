# Historical bug fixes in real projects

Until now every bug in this repository was planted or sampled by me. This is the first test against changes made by other people to real, widely used engines: for each fix commit, build the commit and its parent (unmodified, outside this repository), run the same random scenarios through both over the pipe protocol, and see what the oracles say. Adapters: `examples/external/rebound4_orbit.cpp`, `examples/external/box2d_world.cpp`; builders `scripts/build_rebound_hist.sh`, `scripts/build_box2d_hist.sh`; drivers `scripts/rebound_history.sh`, `scripts/box2d_history.sh`. Raw output: `results/realproj/`.

The commits were picked from `git log` by message and touched files **before** their results were seen (the first two, REBOUND TRACE and Box2D tunneling, were picked after reading the log and diff message, the Box2D restitution and distance commits before any run). I did not choose them for how the oracles would do, and I report every pair I ran. REBOUND is GPLv3 and Box2D MIT; both are cloned and compiled in a scratch directory and nothing of them is in this repository.

| project | change | what the rules saw | what the differential check saw |
|---|---|---|---|
| REBOUND `8b0bfb06` | TRACE integrator bug (close encounters overwritten) | identical (energy_conserved 1/300 in both) | 0/300 differ: **the scenario family never reaches the bug** (needs a dominant star with planets) |
| Box2D `fb8b81f` | "Bug fixes": improved tunneling prevention, restitution in total impulse, graph coloring | counts nearly identical (no_tunneling 52 vs 53 of 1000 with continuous off; 63 vs 63 wall penetrations with it on) | 254/1000 (on) and 276/1000 (off) scenarios differ |
| Box2D `0217969` / `25b971d` | restitution reworked ("Deferred restitution", "Restitution again") | counts move in both directions (in_arena 304 -> 277, then 277 -> 299) | 997/1000 differ (the whole bounce model changed; these are rework, not clean bug fixes) |
| Box2D `3a4f0da` | removed an unnecessary tolerance test in the distance code | identical | 0/1000 differ (not reached) |
| REBOUND releases 4.0.0 .. 4.6.0 (8 builds, 5 integrators, 300 scenarios per pair) | whole-release regression sweep | n/a | only one pair differs: 4.0.0 -> 4.1.0 for IAS15 (28/300) and MERCURIUS (45/300). The commit log lists "Changed default IAS15 timestepping criterion" in that window, so this is most likely an intended change, which I did not confirm |

## Follow-up: scenarios built to reach the REBOUND TRACE fix

The generic generator never reached it, so I wrote star-plus-planets scenarios (`results/realproj/rebound_trace_reach.txt`). Light planets on near-circular orbits: still 0 of 200 differ. Crowded giant planets: the fix is reached (80 of 200 scenarios differ), but the energy rule fires in 199 of 200 on **both** builds and the final energy drift is no better after the fix (smaller in 35, larger in 45). So even when the code is reached, a conservation rule cannot say the fixed build is better; it needs a reference for what TRACE should do, which is the oracle gap again.

## What it shows, and what it does not

- **The rule oracle cannot tell a fixed build from a buggy one** on the one real fix it can reach (Box2D tunneling): the counts are within noise. The declared rules are coarse, with tolerances written for the simulators in this repository. For the restitution commits the counts move but not toward "fixed", because those commits redesigned the behavior.
- **The differential check notices that something changed (25% to 99.7% of scenarios) but cannot say which side is right.** It is a regression-detection tool; a human has to decide. That is the same oracle gap the rest of the repository measures, now on real code.
- **Two of five pairs never reach the changed code at all.** A fixed scenario family only finds bugs it can trigger. This is a limit of the generator, and a real project would need scenarios from its own users.
- **This is not a bug-finding result.** It found no bug in REBOUND or Box2D, and the one behavior difference between REBOUND releases is probably intentional. It is evidence about how far my oracles reach on real changes: not far, for rules; as far as "something changed", for differential tests.
- Small: five pairs plus a release sweep, two projects, one domain each (orbit, rigid2d), one scenario family, one scenario-count (300 or 1000).
