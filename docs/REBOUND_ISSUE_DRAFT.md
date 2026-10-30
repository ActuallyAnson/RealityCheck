# Draft upstream issue (NOT filed; the owner decides whether to post it)

**Title:** IAS15 (default adaptive mode) never returns on a symmetric figure-eight: timestep collapses to ~1e-33

**Version:** REBOUND 5.2.2-3, commit 262a9cce (current `main` when checked on 2026-10-31), macOS, clang, `-O2` and `-O1 -g`.

**What happens:** `reb_simulation_integrate(r, 0.01)` with IAS15 and its default adaptive mode never returns. Stepping manually shows `dt` falling from 0.01 to 4e-17 within the first step and then to about 1e-33 while `t` stays at 2.2e-15. The problem is smooth: three equal masses (G=1) 2 to 4 length units apart, accelerations of order 0.1.

**Reproduction:** `examples/external/rebound_ias15_hang.c` (standalone, only REBOUND's public API: figure-eight with positions (8.06,6.49), (11.94,5.51), (10,6), velocities (0.3297,0.3057) twice and (-0.6593,-0.6115); `softening` 0.3, `dt` 0.01) and `examples/external/rebound_ias15_dt_trace.c` (prints `t` and `dt`).

**Observations:**
- Shifting one body by 1e-16, 1e-14 or 1e-12 still hangs; 1e-10 or more finishes at once. So it needs near-exact symmetry.
- `adaptive_mode` 0 or 1 finishes at once; only the default (PRS23) hangs. Softening 0, 0.1, 0.3 and 1.0 behave the same. BS and leapfrog on the same input are fine.
- I did not read the PRS23 code, so I do not know why the criterion degenerates; I suspect a quantity that vanishes by symmetry.

**Found with:** RealityCheck (https://github.com/ActuallyAnson/RealityCheck), a simulation test harness; the reproduction file contains none of its code.
