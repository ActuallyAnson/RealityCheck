# Third-party software and data

Nothing below is included in this repository's source tree; it is used only as an external program or library at run time, if you build it yourself.

| Software | Version tested | License | How it is used |
|---|---|---|---|
| Box2D | 3.2.0 (Homebrew) | MIT | optional rigid-body engine under test (`box2d`, `box2d-noccd` engines); linked only when found at configure time |
| REBOUND | 5.2.2 (commit 262a9cc) | GPLv3 | N-body library under test through `examples/external/rebound_orbit.cpp` and `scripts/build_rebound_example.sh`; built outside this repository; not redistributed here |
| Chipmunk2D | 7.0.3 (Homebrew) | MIT | tried as an engine, hung in `cpSpaceStep` with a static shape, dropped (`docs/EXTERNAL_ENGINE.md`) |
| curl | system | curl license | the Meta Model API client shells out to it |

No third-party code is compiled into the core. The viewer uses no external scripts, fonts or network resources. The external scenarios in `data/external_scenarios/` are written for this
repository from published initial conditions and standard parameters (citations in their README); no dataset was downloaded. The Meta Model API is used under its own terms.
