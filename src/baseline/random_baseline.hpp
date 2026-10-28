// Random-testing baseline: uniform scenario sampling over the public scenario limits, no model.
#pragma once
#include <chrono>
#include <string>

#include "rules/checker.hpp"
#include "sim/scenario.hpp"
#include "sim/world.hpp"

namespace rc {

// Valid random scenario drawn from the public limits; deterministic in the rng state.
Scenario random_scenario(uint64_t seed);

struct Budget {
  long long max_steps = 300000;  // total simulated outer steps
  double max_wall_ms = 60000;    // wall-clock cap, whichever hits first stops the run
  std::string domain = "rigid2d";  // which scenario space to sample
};

struct HuntResult {
  bool found = false;
  int scenarios_run = 0;
  long long steps_used = 0;
  double wall_ms = 0;
  int scenarios_until_first = 0;  // 1-based, 0 if none
  Scenario first_failure;
  Violation first_violation;
};

HuntResult random_baseline(const SimFn& sim, uint64_t seed, const Budget& budget);
inline HuntResult random_baseline(BugMask bugs, uint64_t seed, const Budget& budget) {
  return random_baseline(builtin_sim(bugs), seed, budget);
}

}  // namespace rc
