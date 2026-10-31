// Reproduction found while testing rc_scan on Chipmunk2D master f2f3d66 (MIT, built outside this repository; no RealityCheck code here).
// Linked against the library built with Chipmunk's own default CMake Release profile (which adds -ffast-math) this hangs in the first cpSpaceStep; linked against
// the same sources built without -ffast-math it finishes. The sleeping feature is documented as disabled by default (sleepTimeThreshold = INFINITY).
// Minimal: default cpSpace (sleeping disabled per docs), one ball resting on a static segment, a few steps.
#include <chipmunk/chipmunk.h>
#include <stdio.h>
int main(void){
  cpSpace*s=cpSpaceNew(); cpSpaceSetGravity(s,cpv(0,-10));
  cpShape*f=cpSpaceAddShape(s,cpSegmentShapeNew(cpSpaceGetStaticBody(s),cpv(-10,0),cpv(10,0),0)); cpShapeSetFriction(f,1);
  for(int i=0;i<2;i++){
    cpBody*b=cpSpaceAddBody(s,cpBodyNew(1,cpMomentForCircle(1,0,0.5,cpvzero))); cpBodySetPosition(b,cpv(i*0.6,0.4));
    cpSpaceAddShape(s,cpCircleShapeNew(b,0.5,cpvzero));
  }
  fprintf(stderr,"sleepTimeThreshold=%g\n",cpSpaceGetSleepTimeThreshold(s));
  for(int k=0;k<5;k++){fprintf(stderr,"step %d\n",k);cpSpaceStep(s,1.0/60);}
  fprintf(stderr,"done\n");return 0;}
