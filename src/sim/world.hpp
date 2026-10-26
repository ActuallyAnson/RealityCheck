// Deterministic 2D physics world: circles, static boxes, an arena and one controller-driven body.
#pragma once
#include <cstdint>
#include <vector>

#include "sim/bugs.hpp"
#include "sim/scenario.hpp"

namespace rc {

struct BodyState {
  double x = 0, y = 0, vx = 0, vy = 0;
};

// State after outer step k (k = 0 is the initial state).
struct StepRecord {
  std::vector<BodyState> body;
  double energy = 0;   // kinetic + potential energy of all bodies
  double work = 0;     // cumulative work done by the controller force
  double fmag = 0;     // largest controller force magnitude applied during the step
  int substeps = 1;
  int waypoint = 0;
};

struct Trace {
  std::vector<StepRecord> rec;
  int steps() const { return static_cast<int>(rec.size()) - 1; }
  uint64_t hash() const;  // hash over the exact bit patterns of every recorded value
};

// Pure function of (scenario, bugs). The scenario must pass validate().
Trace simulate(const Scenario& sc, BugMask bugs);

}  // namespace rc
