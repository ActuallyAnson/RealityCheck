// Reproduction found with RealityCheck: REBOUND 5.2.2 (commit 262a9cc), IAS15 with its default adaptive_mode does not return from a single
// reb_simulation_integrate(r, 0.01) on the figure-eight initial condition below (equal masses; the same with softening 0, 0.1, 0.3, 1.0).
// With adaptive_mode 0 or 1 it finishes at once. This file contains no RealityCheck code.
// build: cc rebound_ias15_hang.c <REBOUND object files> -I<rebound>/src -lm -o hang      (see scripts/build_rebound_example.sh)
// run:   ./hang 0.3 0 0 0.01          (hangs)      ./hang 0.3 0 0 0.01 0   (finishes: adaptive_mode 0)
#include <stdio.h>
#include <stdlib.h>
#include "rebound.h"
int main(int argc, char** argv) {
    double soft = argc > 1 ? atof(argv[1]) : 0.3, dx3 = argc > 2 ? atof(argv[2]) : 0.0, shift = argc > 3 ? atof(argv[3]) : 0.0, dt0 = argc > 4 ? atof(argv[4]) : 0.01;
    struct reb_simulation* r = reb_simulation_create();
    struct reb_integrator_ias15_state* st = (struct reb_integrator_ias15_state*)reb_simulation_set_integrator(r, "ias15");
    if (argc > 5) st->adaptive_mode = atoi(argv[5]);
    r->G = 1.0; r->softening = soft; r->dt = dt0;
    struct reb_particle p[3] = {{0},{0},{0}};
    p[0].x=8.05999128; p[0].y=6.48617506; p[0].vx=0.3296557870776571; p[0].vy=0.3057287396356719; p[0].m=1.0;
    p[1].x=11.94000872; p[1].y=5.51382494; p[1].vx=0.3296557870776571; p[1].vy=0.3057287396356719; p[1].m=1.0;
    p[2].x=10.0; p[2].y=6.0; p[2].vx=-0.6593115741553142; p[2].vy=-0.6114574792713438; p[2].m=1.0;
    p[2].x += dx3;
    for (int i = 0; i < 3; i++) { p[i].x += shift; p[i].y += shift; }
    for (int i = 0; i < 3; i++) reb_simulation_add(r, p[i]);
    reb_simulation_integrate(r, 0.01);
    printf("finished: t=%g\n", r->t);
    reb_simulation_free(r);
    return 0;
}
