# Planted-mutant recall test on Box2D v2.4.1 (pre-registered addendum 2026-11-02)

All files under this directory. Nothing was copied into the RealityCheck repo. Tools: frozen `rc_scan_mut` and `rc_mutgen` (scratchpad/agents).

## Procedure as run

1. Private source copy: `tree/` (copy of box2d24/src); build dir `build/` (fresh cmake Release, `-DBOX2D_BUILD_{DOCS,TESTBED,UNIT_TESTS}=OFF -DBUILD_SHARED_LIBS=OFF`, flags -O3 -DNDEBUG); originals in `orig/`.
2. Line ranges chosen before generating anything: `ranges.txt` (function bodies of the solver / collision / TOI code in the five files; asserts, B2_DEBUG_SOLVER blocks, `#if 0` block, b2_toi profiling counters/timers, b2Island constructor/destructor and Report excluded).
3. Pooling: `rc_mutgen --file F --ranges R --count 100000 --seed 1 --order 1` per file (all sites listed, in rc_mutgen's own seed-1 shuffle), `gen/<file>/`. Pool = the 563 sites (contact_solver 325, island 79, collide_circle 48, collide_polygon 50, time_of_impact 61), concatenated in that file order into `pool.tsv`, then Fisher-Yates shuffled with `awk 'BEGIN{srand(1)}...'` into `order.tsv`. Mutants = first 40 rows of `order.tsv` that compile (`build_mutants.sh`); rows skipped for compile failure are logged in `compile_failures.tsv`, the next row in the fixed order is taken (positions 24, 28, 36 failed: `float * Vec2 -> float / Vec2` has no operator; 3 skipped, 43 sites walked). Selected list with pool position: `selected.tsv`. Each mutant: mutated file copied over the private tree, `cmake --build build -j1` under `nice` (incremental), adapter relinked (`link.sh`) to `bins/Mxx`, original restored.
4. Behaviour: `genreq.sh` -> `requests.jsonl` (300 requests, seeds 1..300, all 15 parameters uniform in the scan ranges, steps 200, awk srand(20261101)); `behave.sh BIN OUT` runs one process per request with `timeout 20`; outputs in `behave/`; unchanged = byte-identical to `behave/clean.out`; crashed/hung = any request TIMEOUT or non-zero exit. (`class.txt`)
5. Scans (`scan_one.sh`, `scan_all.sh`): clean build and every changed or crashed mutant, arms A and B; unchanged mutants were not scanned (classed unchanged). Reports: `scans/<id>_<A|B>/report.md`; exit codes `scans/exit_codes.txt`.
6. Classification: `keys.sh`, `classify.sh` -> `classification.tsv`; table below from `table.md`.

Scan command (base scan A of box2d24/cmds.txt: same parameter ranges, `--scenarios 600` instead of 800, `--steps 200`, `--name`/`--out` per run), verbatim in `scan_one.sh`:

    export RC_EXEC_TIMEOUT_S=10
    nice rc_scan_mut --cmd bins/ID --param balls:1:30:int --param size:0.1:1.2 --param restitution:0:1 --param friction:0:1 --param density:0.1:10:log --param gravity:0:30 --param speed:0:60 --param dt:0.004:0.05 --param viter:1:12:int --param piter:1:6:int --param sleep:0:1:int --param shape:0:2:int --param joints:0:1:int --param bullet:0:1:int --param ccd:0:1:int --steps 200 --scenarios 600 [arm B: --require "n_outside<=0"] --out scans/ID_ARM --name ID_ARM

## Clean build

Arm A: 1 finding (learned guess `n_outside <= 12 (mined)`, 3 of 600). Arm B: 2 findings (your rule `n_outside <= 0`, 133 of 600; and the same learned guess). So the clean build gives 1 false finding in arm A and 2 in arm B (the B rule is violated by the clean engine: n_outside counts body centres outside the 20x20 box, bodies fly out at high speed/small size). These are present in every report and are not counted as detections.

## Totals (40 mutants, none dropped except the 3 compile failures replaced as pre-declared)

| class | count |
|---|---|
| unchanged on 300 requests | 9 (M01 M15 M19 M25 M26 M32 M35 M37 M39) |
| crashed or hung | 2 (M11: segfault on the first/probe request; M13: segfault on 84 of 300 requests, loop `i <= m_bodyCount`) |
| changed | 29 |

Noticed means: a finding row (type + text) absent from the clean report of the same arm (addendum, literal reading). Because mined limits are fitted to each mutant's own normal runs, a learned guess with a shifted number counts as new text. I therefore also give a "strict" count (numbers in the text ignored for learned guesses; hard failures and user-rule rows must be absent from clean).

| | arm A | arm B |
|---|---|---|
| changed mutants noticed, literal (primary) | 17 / 29 | 17 / 29 |
| changed mutants noticed, strict | 15 / 29 | 15 / 29 |
| of those, by a HARD FAILURE (nondeterministic or non-finite signal) | 6 (M04 M10 M17 M27 M31 M36) | 6 |
| crashed/hung mutants reported by the scan | 2 / 2 (M11: scan aborts with exit 2 "first output line is not JSON ... Segmentation fault", no report; M13: HARD FAILURE `run failed` + `nondeterministic`) | 2 / 2 |

Arm B adds nothing: the extra rule `n_outside<=0` is already violated by the clean build, so the same row appears in every report and no mutant produced a new row for it (and counts per mutant are identical to arm A except the number of rows).

Noticed only by a shifted-number learned guess (literal yes, strict no): M05, M24.
Note: 11 of the 17 noticed changed mutants (literal) rest only on learned-invariant rows, which the scan itself calls candidates; 6 are hard failures.

## Missed changed mutants (13 of 29 in both arms; no new finding)

M06 b2_collide_polygon.cpp:204 `if (np < 2)` -> `np < 3`; M07 b2_island.cpp:380 `for (i = 0; ...)` -> `i = 1` (sleep-time loop skips body 0); M08 b2_contact_solver.cpp:216 `vB + cross - vA` -> `+ vA` (vRel used only for restitution bias); M14 b2_contact_solver.cpp:224 `pointCount == 2 && g_blockSolve` -> `== 3` (block solver disabled); M18 b2_time_of_impact.cpp:281 `totalRadius - 3*slop` -> `3 / slop`; M21 b2_contact_solver.cpp:414 `points + 0` -> `+ 1`; M22 b2_contact_solver.cpp:201 `iA * rnA * rnA` -> `iA / rnA * rnA`; M23 b2_contact_solver.cpp:819 `iB * rnB * rnB` -> `iB * rnB / rnB`; M28 b2_island.cpp:467 `i < velocityIterations` -> `<=` (TOI sub-step, one extra iteration); M29 and M30 b2_contact_solver.cpp:779 `indexB == toiIndexA || indexB == toiIndexB` -> `|| indexB != toiIndexB` (M29) and `&&` (M30); M33 b2_time_of_impact.cpp:417 `rootIterCount & 1` -> `& 2`; M38 b2_island.cpp:323 `jointsOkay && jointOkay` -> `||`.

Kinds missed: (a) mutants inside the continuous-collision (TOI) path (M18, M28, M29, M30, M33), (reached only with ccd enabled; I did not verify the cause of each miss); (b) rarely reached or tolerance-level branches (M06 polygon clipping count, M14 block solver off, M38 joint-position convergence flag, M21 second-point velocity constraint), (my reading, not verified: the simulation stays plausible); (c) arithmetic that changes solver quality but not a conserved or bounded quantity (M08 restitution bias velocity, M22/M23 effective mass terms). The scan finds mutants that blow up (non-finite, nondeterminism after invalid memory reads, crashes) or that make quantities drift to extreme magnitudes; it does not find mutants that leave the world plausible.

## Per-mutant table

Columns A/B literal = number of finding rows absent from the clean report of that arm; strict = same with numbers in learned guesses ignored. 'first new finding' is the first such row.

| id | file:line | original line -> mutated line | change | class | A literal (new rows) | A strict | B literal | B strict | first new finding, arm A |
|---|---|---|---|---|---|---|---|---|---|
| M01 | collision/b2_time_of_impact.cpp:286 | `	const int32 k_maxIterations = 20;	` -> `	const int32 k_maxIterations = 21;	` | constant 20 -> 21 | unchanged | - | - | - | - | -:- |
| M02 | dynamics/b2_contact_solver.cpp:348 | `			wB += iB * b2Cross(vcp->rB, P);` -> `			wB -= iB * b2Cross(vcp->rB, P);` | operator '+=' -> '-=' | changed | 4 | 3 | 4 | 3 | learned guess:breaks learned invariant: max_speed <= 360.061 (mined):learned guess:breaks learned invariant: max_speed <= 360.061 (mined) |
| M03 | dynamics/b2_contact_solver.cpp:236 | `			float k12 = mA + mB + iA * rn1A * rn2A + iB * rn1B * rn2B;` -> `			float k12 = mA + mB - iA * rn1A * rn2A + iB * rn1B * rn2B;` | operator '+' -> '-' | changed | 1 | 1 | 1 | 1 | learned guess:breaks learned invariant: max_speed stays within 9.32377 x (\|its start\| + 5.07439) of its own st:learned guess:breaks learned invariant: max_speed stays within 9.32377 x (\|its start\| + 5.07439) of its own st |
| M04 | collision/b2_collide_polygon.cpp:124 | `	manifold->pointCount = 0;` -> `	manifold->pointCount = 1;` | constant 0 -> 1 | changed | 5 | 5 | 6 | 5 | HARD FAILURE:nondeterministic:HARD FAILURE:nondeterministic |
| M05 | dynamics/b2_contact_solver.cpp:240 | `			if (k11 * k11 < k_maxConditionNumber * (k11 * k22 - k12 * k12))` -> `			if (k11 * k11 < k_maxConditionNumber / (k11 * k22 - k12 * k12))` | operator '*' -> '/' | changed | 1 | 0 | 1 | 0 | learned guess:breaks learned invariant: n_outside <= 13.5 (mined):learned guess:breaks learned invariant: n_outside <= 13.5 (mined) |
| M06 | collision/b2_collide_polygon.cpp:204 | `	if (np < 2)` -> `	if (np < 3)` | constant 2 -> 3 | changed | 0 | 0 | 0 | 0 | : |
| M07 | dynamics/b2_island.cpp:380 | `			for (int32 i = 0; i < m_bodyCount; ++i)` -> `			for (int32 i = 1; i < m_bodyCount; ++i)` | constant 0 -> 1 | changed | 0 | 0 | 0 | 0 | : |
| M08 | dynamics/b2_contact_solver.cpp:216 | `			float vRel = b2Dot(vc->normal, vB + b2Cross(wB, vcp->rB) - vA - b2Cross(wA, vcp->rA));` -> `			float vRel = b2Dot(vc->normal, vB + b2Cross(wB, vcp->rB) + vA - b2Cross(wA, vcp->rA));` | operator '-' -> '+' | changed | 0 | 0 | 0 | 0 | : |
| M09 | dynamics/b2_contact_solver.cpp:751 | `	return minSeparation >= -3.0f * b2_linearSlop;` -> `	return minSeparation >= -3.0f / b2_linearSlop;` | operator '*' -> '/' | changed | 1 | 1 | 1 | 1 | learned guess:breaks learned invariant: kinetic stays within 1.89669 x (\|its start\| + 58117) of its own start :learned guess:breaks learned invariant: kinetic stays within 1.89669 x (\|its start\| + 58117) of its own start  |
| M10 | dynamics/b2_contact_solver.cpp:736 | `			aA -= iA * b2Cross(rA, P);` -> `			aA -= iA / b2Cross(rA, P);` | operator '*' -> '/' | changed | 5 | 4 | 5 | 4 | HARD FAILURE:non-finite signal kinetic:HARD FAILURE:non-finite signal kinetic |
| M11 | dynamics/b2_contact_solver.cpp:193 | `			b2VelocityConstraintPoint* vcp = vc->points + j;` -> `			b2VelocityConstraintPoint* vcp = vc->points - j;` | operator '+' -> '-' | crashed (probe segfault) | abort+report | abort+report | abort+report | abort+report | rc_scan: first output line is not JSON: Segmentation fault (exit 2, no report.md) |
| M12 | dynamics/b2_contact_solver.cpp:422 | `			b2Vec2 dv2 = vB + b2Cross(wB, cp2->rB) - vA - b2Cross(wA, cp2->rA);` -> `			b2Vec2 dv2 = vB - b2Cross(wB, cp2->rB) - vA - b2Cross(wA, cp2->rA);` | operator '+' -> '-' | changed | 3 | 2 | 3 | 2 | learned guess:breaks learned invariant: n_outside <= 10.5 (mined):learned guess:breaks learned invariant: n_outside <= 10.5 (mined) |
| M13 | dynamics/b2_island.cpp:478 | `	for (int32 i = 0; i < m_bodyCount; ++i)` -> `	for (int32 i = 0; i <= m_bodyCount; ++i)` | operator '<' -> '<=' | crashed (84/300 requests) | 10 | 10 | 6 | 6 | HARD FAILURE:nondeterministic:HARD FAILURE:nondeterministic |
| M14 | dynamics/b2_contact_solver.cpp:224 | `		if (vc->pointCount == 2 && g_blockSolve)` -> `		if (vc->pointCount == 3 && g_blockSolve)` | constant 2 -> 3 | changed | 0 | 0 | 0 | 0 | : |
| M15 | dynamics/b2_contact_solver.cpp:536 | `				if (x.y >= 0.0f && vn1 >= 0.0f)` -> `				if (x.y >= 0.0f && vn1 > 0.0f)` | operator '>=' -> '>' | unchanged | - | - | - | - | -:- |
| M16 | dynamics/b2_contact_solver.cpp:544 | `					vA -= mA * (P1 + P2);` -> `					vA += mA * (P1 + P2);` | operator '-=' -> '+=' | changed | 3 | 3 | 3 | 3 | learned guess:breaks learned invariant: kinetic stays within 1.89671 x (\|its start\| + 58117) of its own start :learned guess:breaks learned invariant: kinetic stays within 1.89671 x (\|its start\| + 58117) of its own start  |
| M17 | dynamics/b2_contact_solver.cpp:822 | `			float impulse = K > 0.0f ? - C / K : 0.0f;` -> `			float impulse = K > 0.0f ? - C * K : 0.0f;` | operator '/' -> '*' | changed | 9 | 9 | 9 | 9 | HARD FAILURE:non-finite signal kinetic:HARD FAILURE:non-finite signal kinetic |
| M18 | collision/b2_time_of_impact.cpp:281 | `	float target = b2Max(b2_linearSlop, totalRadius - 3.0f * b2_linearSlop);` -> `	float target = b2Max(b2_linearSlop, totalRadius - 3.0f / b2_linearSlop);` | operator '*' -> '/' | changed | 0 | 0 | 0 | 0 | : |
| M19 | collision/b2_collide_circle.cpp:112 | `	if (u1 <= 0.0f)` -> `	if (u1 < 0.0f)` | operator '<=' -> '<' | unchanged | - | - | - | - | -:- |
| M20 | dynamics/b2_contact_solver.cpp:210 | `			float kTangent = mA + mB + iA * rtA * rtA + iB * rtB * rtB;` -> `			float kTangent = mA - mB + iA * rtA * rtA + iB * rtB * rtB;` | operator '+' -> '-' | changed | 1 | 1 | 1 | 1 | learned guess:breaks learned invariant: max_abs_x <= 203.623 (mined):learned guess:breaks learned invariant: max_abs_x <= 203.623 (mined) |
| M21 | dynamics/b2_contact_solver.cpp:414 | `			b2VelocityConstraintPoint* cp1 = vc->points + 0;` -> `			b2VelocityConstraintPoint* cp1 = vc->points + 1;` | constant 0 -> 1 | changed | 0 | 0 | 0 | 0 | : |
| M22 | dynamics/b2_contact_solver.cpp:201 | `			float kNormal = mA + mB + iA * rnA * rnA + iB * rnB * rnB;` -> `			float kNormal = mA + mB + iA / rnA * rnA + iB * rnB * rnB;` | operator '*' -> '/' | changed | 0 | 0 | 0 | 0 | : |
| M23 | dynamics/b2_contact_solver.cpp:819 | `			float K = mA + mB + iA * rnA * rnA + iB * rnB * rnB;` -> `			float K = mA + mB + iA * rnA * rnA + iB * rnB / rnB;` | operator '*' -> '/' | changed | 0 | 0 | 0 | 0 | : |
| M24 | collision/b2_collide_polygon.cpp:194 | `	float sideOffset2 = b2Dot(tangent, v12) + totalRadius;` -> `	float sideOffset2 = b2Dot(tangent, v12) - totalRadius;` | operator '+' -> '-' | changed | 1 | 0 | 1 | 0 | learned guess:breaks learned invariant: n_outside <= 13.5 (mined):learned guess:breaks learned invariant: n_outside <= 13.5 (mined) |
| M25 | collision/b2_collide_polygon.cpp:129 | `	if (separationA > totalRadius)` -> `	if (separationA >= totalRadius)` | operator '>' -> '>=' | unchanged | - | - | - | - | -:- |
| M26 | collision/b2_collide_polygon.cpp:86 | `	for (int32 i = 0; i < count2; ++i)` -> `	for (int32 i = 0; i <= count2; ++i)` | operator '<' -> '<=' | unchanged | - | - | - | - | -:- |
| M27 | dynamics/b2_island.cpp:502 | `		a += h * w;` -> `		a += h / w;` | operator '*' -> '/' | changed | 5 | 5 | 5 | 5 | HARD FAILURE:non-finite signal kinetic:HARD FAILURE:non-finite signal kinetic |
| M28 | dynamics/b2_island.cpp:467 | `	for (int32 i = 0; i < subStep.velocityIterations; ++i)` -> `	for (int32 i = 0; i <= subStep.velocityIterations; ++i)` | operator '<' -> '<=' | changed | 0 | 0 | 0 | 0 | : |
| M29 | dynamics/b2_contact_solver.cpp:779 | `		if (indexB == toiIndexA \|\| indexB == toiIndexB)` -> `		if (indexB == toiIndexA \|\| indexB != toiIndexB)` | operator '==' -> '!=' | changed | 0 | 0 | 0 | 0 | : |
| M30 | dynamics/b2_contact_solver.cpp:779 | `		if (indexB == toiIndexA \|\| indexB == toiIndexB)` -> `		if (indexB == toiIndexA && indexB == toiIndexB)` | operator '||' -> '&&' | changed | 0 | 0 | 0 | 0 | : |
| M31 | dynamics/b2_contact_solver.cpp:151 | `		b2ContactPositionConstraint* pc = m_positionConstraints + i;` -> `		b2ContactPositionConstraint* pc = m_positionConstraints - i;` | operator '+' -> '-' | changed | 11 | 10 | 7 | 6 | HARD FAILURE:nondeterministic:HARD FAILURE:nondeterministic |
| M32 | dynamics/b2_contact_solver.cpp:414 | `			b2VelocityConstraintPoint* cp1 = vc->points + 0;` -> `			b2VelocityConstraintPoint* cp1 = vc->points - 0;` | operator '+' -> '-' | unchanged | - | - | - | - | -:- |
| M33 | collision/b2_time_of_impact.cpp:417 | `				if (rootIterCount & 1)` -> `				if (rootIterCount & 2)` | constant 1 -> 2 | changed | 0 | 0 | 0 | 0 | : |
| M34 | dynamics/b2_contact_solver.cpp:502 | `					vA -= mA * (P1 + P2);` -> `					vA += mA * (P1 + P2);` | operator '-=' -> '+=' | changed | 3 | 2 | 3 | 2 | learned guess:breaks learned invariant: n_outside <= 13.5 (mined):learned guess:breaks learned invariant: n_outside <= 13.5 (mined) |
| M35 | dynamics/b2_contact_solver.cpp:576 | `				if (vn1 >= 0.0f && vn2 >= 0.0f )` -> `				if (vn1 >= 0.0f \|\| vn2 >= 0.0f )` | operator '&&' -> '||' | unchanged | - | - | - | - | -:- |
| M36 | dynamics/b2_contact_solver.cpp:345 | `			wA -= iA * b2Cross(vcp->rA, P);` -> `			wA -= iA / b2Cross(vcp->rA, P);` | operator '*' -> '/' | changed | 7 | 6 | 7 | 6 | HARD FAILURE:non-finite signal kinetic:HARD FAILURE:non-finite signal kinetic |
| M37 | dynamics/b2_island.cpp:221 | `			v *= 1.0f / (1.0f + h * b->m_linearDamping);` -> `			v *= 1.0f * (1.0f + h * b->m_linearDamping);` | operator '/' -> '*' | unchanged | - | - | - | - | -:- |
| M38 | dynamics/b2_island.cpp:323 | `			jointsOkay = jointsOkay && jointOkay;` -> `			jointsOkay = jointsOkay \|\| jointOkay;` | operator '&&' -> '||' | changed | 0 | 0 | 0 | 0 | : |
| M39 | dynamics/b2_island.cpp:289 | `		if (b2Dot(translation, translation) > b2_maxTranslationSquared)` -> `		if (b2Dot(translation, translation) >= b2_maxTranslationSquared)` | operator '>' -> '>=' | unchanged | - | - | - | - | -:- |
| M40 | collision/b2_collide_polygon.cpp:220 | `	for (int32 i = 0; i < b2_maxManifoldPoints; ++i)` -> `	for (int32 i = 1; i < b2_maxManifoldPoints; ++i)` | constant 0 -> 1 | changed | 1 | 1 | 1 | 1 | learned guess:breaks learned invariant: max_abs_x <= 206.561 (mined):learned guess:breaks learned invariant: max_abs_x <= 206.561 (mined) |

## Deviations / notes

- 3 sites failed to compile (positions 24, 28, 36 of `order.tsv`, all `float * Vec2 -> /`) and were replaced by the next positions in the fixed order (as the task allowed); 43 sites walked for 40 mutants.
- Scans of unchanged mutants were not run (classed unchanged).
- Behaviour check used a 20 s per-request limit; scans used RC_EXEC_TIMEOUT_S=10. Some mutants (M04, M10, M17, M27, M36) made most scan scenarios hit the 10 s limit during minimisation, scans took 30 to 90 min each; settings were not changed.
- 'unchanged' is on 300 random requests only; M32 (`points + 0` -> `- 0`) is a true equivalent; M37 changes only the linear-damping term, which the adapter never sets (damping 0), so it is equivalent for this adapter.
- Scan ranges: the base scan A of box2d24/cmds.txt had 800 scenarios; here 600 as pre-registered.

## Correction by the repo owner's re-count (added after import; the table above is untouched)

The agent's summary said 17 of 29 (literal) and 15 of 29 (strict). Re-counting `classification.tsv` gives 16 of 29 (literal) and 14 of 29 (strict): M02 M03 M04 M05 M09 M10 M12 M16 M17 M20 M24 M27 M31 M34 M36 M40. This agrees with the 13 missed mutants listed above (29 - 16 = 13). Hard failures: 6 of the 16.
