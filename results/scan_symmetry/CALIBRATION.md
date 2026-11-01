Calibration on the clean Box2D 2.4.1 build (docs/PREREG_FINAL.md addendum 2026-11-02 (b)), 600 scenarios, grid in `grid.sh`, full report in `calibration_grid_clean_report.md`.
Rule: per relation, the strictest cell (smallest tol, then largest first) with zero false findings.
- mirror: false findings at every cell except first=2, tol=0.2. Chosen: first=2, tol=0.2 (a loose check).
- shift: at tol=0.01 only first=2 has zero false findings. Chosen: first=2, tol=0.01.
Seed-2 mutant pool: 563 sites (same ranges). Shuffle: awk random key with srand(2) (the seed-1 test used an awk Fisher-Yates with srand(1); not identical code, same intent). Three sites failed to compile (positions 17, 31, 32) and were skipped.
