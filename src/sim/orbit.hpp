// Orbit domain: softened Newtonian N-body gravity (KDK leapfrog with adaptive substeps) in the plane.
// A second domain behind the same tools, rules and shrinker. Scenario fields reused: gravity = G,
// bodies {x,y,vx,vy,m}; radius and restitution are unused and not exposed.
#pragma once
#include <string>

#include "sim/bugs.hpp"
#include "sim/scenario.hpp"
#include "sim/world.hpp"

namespace rc {

constexpr double kOrbitSoftening = 0.3;

struct OrbitLimits {
  static constexpr double dt_min = 1e-3, dt_max = 1.0 / 30.0;
  static constexpr int steps_max = 4000;
  static constexpr int bodies_min = 2, bodies_max = 6;
  static constexpr double G_min = 0.5, G_max = 5.0;
  static constexpr double m_min = 0.2, m_max = 20.0;
  static constexpr double speed_max = 10.0;
  static constexpr double min_sep = 1.0;  // initial separation of any two bodies
  static constexpr double arena_w = 20.0, arena_h = 12.0;  // initial positions lie in this box
};

// Seeded orbit bugs; mask bit i = o(i+1). Internal names for sim code only.
constexpr int kNumOrbitBugs = 3;
enum OrbitBug : int { kOrbSubstepCap = 0, kOrbAsymmetricPull = 1, kOrbSkewedSoftening = 2 };
inline std::string orbit_bug_id(int i) { return "o0" + std::to_string(i + 1); }
inline int orbit_bug_index(const std::string& id) {
  for (int i = 0; i < kNumOrbitBugs; ++i) if (orbit_bug_id(i) == id) return i;
  return -1;
}

Json orbit_to_json(const Scenario& s);
Scenario orbit_from_json(const Json& j);
std::string orbit_validate(const Scenario& s);
Trace simulate_orbit(const Scenario& sc, BugMask bugs);
Scenario random_orbit_scenario(uint64_t seed);

// Kinetic and (softened) potential energy of a recorded state, for rules and tools.
void orbit_energies(const Scenario& sc, const StepRecord& r, double* ke, double* pe);

}  // namespace rc
