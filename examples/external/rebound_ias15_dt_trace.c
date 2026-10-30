// Companion to rebound_ias15_hang.c (no RealityCheck code): prints t and dt after each of the first steps of the same figure-eight, and every 500000th step after
// that, with REBOUND 5.2.2-3 (262a9cce) IAS15 in its default adaptive mode. dt collapses from 1e-2 to 1e-17 within one step and then to 1e-33 while t stops moving.
#include <stdio.h>
#include "rebound.h"
int main(){
  struct reb_simulation* r = reb_simulation_create();
  reb_simulation_set_integrator(r, "ias15");
  r->G=1; r->softening=0.3; r->dt=0.01;
  struct reb_particle p[3]={{0},{0},{0}};
  p[0].x=8.05999128;p[0].y=6.48617506;p[0].vx=0.3296557870776571;p[0].vy=0.3057287396356719;p[0].m=1;
  p[1].x=11.94000872;p[1].y=5.51382494;p[1].vx=0.3296557870776571;p[1].vy=0.3057287396356719;p[1].m=1;
  p[2].x=10;p[2].y=6;p[2].vx=-0.6593115741553142;p[2].vy=-0.6114574792713438;p[2].m=1;
  for(int i=0;i<3;i++) reb_simulation_add(r,p[i]);
  for(long k=0;k<3000000;k++){ reb_simulation_steps(r,1); if(k<12 || k%500000==0) printf("%ld t=%.17g dt=%.3g\n",k,r->t,r->dt); }
  return 0; }
