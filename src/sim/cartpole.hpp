// Cart-pole domain: a pole balanced on a cart by a fixed linear state-feedback controller (gains from an
// LQR-style design for a nominal plant), RK4 plant integration, zero-order-hold control at the outer step.
// Trace layout: body 0 = {x: cart position, y: pole angle from upright, vx: cart velocity, vy: pole angular velocity};
// StepRecord::fmag = force the controller applied during the step, work = cumulative work of all external forces.
#pragma once
#include <string>

#include "sim/bugs.hpp"
#include "sim/scenario.hpp"
#include "sim/world.hpp"

namespace rc {

struct CartPoleLimits {
  static constexpr double dt_min = 0.002, dt_max = 0.02;
  static constexpr int steps_max = 4000;
  static constexpr double mc_min = 0.5, mc_max = 2.0, mp_min = 0.05, mp_max = 0.5, l_min = 0.3, l_max = 1.0;
  static constexpr double g_min = 5.0, g_max = 15.0;
  static constexpr double x0_max = 0.5, v0_max = 0.5, th0_max = 0.2, w0_max = 0.5;
  static constexpr double fmax_min = 20.0, fmax_max = 50.0;
  static constexpr double noise_max = 0.02;
  static constexpr int pushes_max = 3;
  static constexpr double push_force_max = 10.0, push_len_max = 0.2;
};

// Seeded bugs (mask bit i = c(i+1)); internal names for sim code only.
constexpr int kNumCartBugs = 3;
enum CartBug : int { kCartDroppedRate = 0, kCartLooseClamp = 1, kCartPositionSign = 2 };
inline std::string cart_bug_id(int i) { return "c0" + std::to_string(i + 1); }
inline int cart_bug_index(const std::string& id) {
  for (int i = 0; i < kNumCartBugs; ++i) if (cart_bug_id(i) == id) return i;
  return -1;
}

Json cartpole_to_json(const Scenario& s);
Scenario cartpole_from_json(const Json& j);
std::string cartpole_validate(const Scenario& s);
Trace simulate_cartpole(const Scenario& sc, BugMask bugs);
Scenario random_cartpole_scenario(uint64_t seed);

// Mechanical energy (kinetic of cart and pole tip plus pole potential) of a recorded state.
double cartpole_energy(const CartPoleDef& cp, const BodyState& q);

}  // namespace rc
