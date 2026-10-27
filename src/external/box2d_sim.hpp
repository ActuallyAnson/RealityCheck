// Adapter that runs a Scenario in an unmodified Box2D (v3) world and returns the same Trace our rules check.
// Box2D is an outside engine: nothing here is seeded with bugs. Controller scenarios are not supported
// (the controller is ignored), so the force-limit rule cannot fire.
#pragma once
#include "sim/world.hpp"

namespace rc {

struct Box2DOptions {
  int substeps = 4;          // Box2D solver substeps per outer step (Box2D's own default)
  bool continuous = true;    // Box2D continuous collision, on by default in the engine
};

SimFn box2d_sim(Box2DOptions opt = Box2DOptions());

}  // namespace rc
