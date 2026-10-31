// Standalone, public API only: one dynamic box dropped on a btStaticPlaneShape (or a big static box). Args: dt halfExtent vx dropHeight steps useBoxFloor
#include <btBulletDynamicsCommon.h>
#include <cstdio>
#include <cstdlib>
int main(int argc, char** argv) {
  double dt = argc > 1 ? atof(argv[1]) : 1.0 / 30, h = argc > 2 ? atof(argv[2]) : 0.2, vx = argc > 3 ? atof(argv[3]) : 5, y0 = argc > 4 ? atof(argv[4]) : 8;
  int steps = argc > 5 ? atoi(argv[5]) : 200, boxfloor = argc > 6 ? atoi(argv[6]) : 0;
  btDefaultCollisionConfiguration cfg; btCollisionDispatcher disp(&cfg); btDbvtBroadphase bp; btSequentialImpulseConstraintSolver sol;
  btDiscreteDynamicsWorld w(&disp, &bp, &sol, &cfg);
  w.setGravity(btVector3(0, -10, 0));
  btStaticPlaneShape plane(btVector3(0, 1, 0), 0);
  btBoxShape bigbox(btVector3(50, 1, 50));
  btCollisionShape* fs = boxfloor ? (btCollisionShape*)&bigbox : (btCollisionShape*)&plane;
  btTransform t; t.setIdentity(); if (boxfloor) t.setOrigin(btVector3(0, -1, 0));
  btDefaultMotionState ms0(t); btRigidBody::btRigidBodyConstructionInfo ci0(0, &ms0, fs, btVector3(0, 0, 0));
  btRigidBody floor(ci0); w.addRigidBody(&floor);
  btBoxShape box(btVector3(h, h, h)); btVector3 in; box.calculateLocalInertia(1, in);
  t.setOrigin(btVector3(10, y0, 10)); btDefaultMotionState ms(t);
  btRigidBody::btRigidBodyConstructionInfo ci(1, &ms, &box, in); btRigidBody b(ci);
  b.setLinearVelocity(btVector3(vx, 0, 0)); w.addRigidBody(&b);
  double miny = 1e9;
  for (int i = 0; i < steps; ++i) { w.stepSimulation((btScalar)dt, 1, (btScalar)dt); double y = b.getWorldTransform().getOrigin().y(); if (i > steps / 2 && y < miny) miny = y; }
  printf("dt=%g half=%g vx=%g floor=%s: final y=%.4f (resting center should be ~%.2f), min y over 2nd half=%.4f\n", dt, h, vx, boxfloor ? "box" : "plane", b.getWorldTransform().getOrigin().y(), h, miny);
}
