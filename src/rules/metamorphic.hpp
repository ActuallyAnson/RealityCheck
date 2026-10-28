// Metamorphic oracles: relations between runs that must hold for a correct simulator, with no knowledge of any particular bug.
// The set is fixed (docs/MUTATION.md) and its tolerances are calibrated on the unmodified simulator only.
//   prefix   a run of N/2 steps equals the first N/2 steps of the N-step run (exact; all domains)
//   repeat   running the same scenario twice gives identical traces (exact; all domains)
//   perm     reversing the body order does not change the physics (orbit; rigid2d without controller)
//   mirror   reflecting x -> W - x (or the cart-pole's x, theta -> -x, -theta) reflects the trajectory (orbit; cart-pole without noise; rigid2d without controller)
//   refine   halving the step gives the same early trajectory within a calibrated tolerance (orbit, cart-pole; rigid2d without controller)
#pragma once
#include <string>
#include <vector>

#include "sim/scenario.hpp"
#include "sim/world.hpp"

namespace rc {

struct MetamorphicDeviation {  // largest deviation seen per relation (for calibration); -1 when the relation did not apply
  double perm = -1, mirror = -1, refine = -1;
  bool prefix_ok = true, repeat_ok = true;
};

// Runs the relations on `sc` through `sim`; fills the deviations and returns the ids of the relations that fail at the calibrated tolerances.
std::vector<std::string> metamorphic_failures(const Scenario& sc, const SimFn& sim, MetamorphicDeviation* dev = nullptr);

}  // namespace rc
