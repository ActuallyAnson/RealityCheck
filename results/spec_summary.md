| domain | invariants kept (clean + held-out) | rejected on clean runs | bug | model-written invariants fired | hand-written rules fired | invariants that fired |
|---|---|---|---|---|---|---|
| rigid2d | 10 | 1 | b01 | 0/300 | 19/300 |  |
|  |  |  | b02 | 0/300 | 1/300 |  |
|  |  |  | b03 | 1/300 | 6/300 | energy-upper |
|  |  |  | b04 | 0/300 | 127/300 |  |
|  |  |  | b05 | 29/300 | 39/300 | energy-lower, energy-upper, gap-arena-size_r2, speed-cap, speed-nonneg |
|  |  |  | b06 | 0/300 | 0/300 |  |
|  |  |  | b07 | 0/300 | 15/300 |  |
|  |  |  | b08 | 48/300 | 48/300 | ctrl-limit |
|  |  |  | b09 | 2/300 | 2/300 | energy-lower, energy-upper |
|  |  |  | b10 | 0/300 | 151/300 |  |
|  |  |  | b11 | 0/300 | 10/300 |  |
|  |  |  | b12 | 0/300 | 0/300 |  |
|  |  |  | b13 | 1/300 | 1/300 | ctrl-limit |
| orbit | 12 | 0 | o01 | 0/300 | 24/300 |  |
|  |  |  | o02 | 28/300 | 26/300 | angmom_cons, momx_cons, momy_cons |
|  |  |  | o03 | 22/300 | 22/300 | angmom_cons |
| cartpole | 12 | 0 | c01 | 34/300 | 34/300 | cartv-lower, cartv-upper, energy-upper, omega-lower, omega-upper, pole-upright-lower, pole-upright-upper |
|  |  |  | c02 | 0/300 | 0/300 |  |
|  |  |  | c03 | 10/300 | 10/300 | cartv-lower, cartv-upper, energy-upper, omega-lower, omega-upper, pole-upright-lower, pole-upright-upper |
| traffic | 12 | 0 | t01 | 0/300 | 50/300 |  |
|  |  |  | t02 | 0/300 | 24/300 |  |
|  |  |  | t03 | 0/300 | 2/300 |  |

Bugs hit at least once in the hunt: model-written invariants 9/22, hand-written rules 19/22. Model cost $0.0679471.
