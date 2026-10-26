# RealityCheck

An AI-driven adversarial tester for simulations. A model hunts for situations where a deterministic
2D physics simulation breaks its declared rules; the engine, not the model, decides what counts as a
violation. Failures are shrunk to a minimal scenario, replayed, and explained with cited steps.

Entry for the Meta Global AI Developer Hackathon (Indie Developer track, "Push the Frontier").

> Status: work in progress. Every number in this README will be produced by a script in this repo.

## Build

    cmake -S . -B build && cmake --build build -j && ctest --test-dir build

See `WHAT_WE_BUILT.md` for what was created during the hackathon. License: MIT.
