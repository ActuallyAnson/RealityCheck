# Contributing

- Build and test: `cmake -S . -B build && cmake --build build -j && ./build/rc_tests` (771+ checks; plain C++17, no dependencies). CI builds on Ubuntu and macOS and runs the tests under ASan/UBSan plus a fuzz pass.
- Rules the project holds itself to: no secrets in the repository (the key lives in `MODEL_API_KEY`); no ground truth in any prompt the tester sees; every number in the README or docs comes from a script in the repo;
  a new behaviour of the agent goes behind a version flag so frozen results stay reproducible; negative and failed attempts are documented, not deleted.
- Adding a domain: `docs/ADD_A_DOMAIN.md`. Adding an engine: speak `docs/PROTOCOL.md`. Testing a program without C++: `docs/GENERIC_DOMAIN.md`.
- Evaluations that spend money need `--live`, a lock file, and cap flags; run `./build/rc spend` first.
- Please do not commit generated PNGs, build directories or run outputs unless they are the saved results of a documented evaluation.
