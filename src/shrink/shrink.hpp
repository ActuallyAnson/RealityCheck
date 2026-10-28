// Greedy delta-debugging shrinker. A candidate is accepted only if it still violates the same rule,
// so the returned scenario always reproduces the failure (checked again before returning).
#pragma once
#include <functional>
#include <string>

#include "rules/checker.hpp"
#include "sim/bugs.hpp"
#include "sim/scenario.hpp"
#include "sim/world.hpp"

namespace rc {

struct ShrinkResult {
  bool ok = false;  // false when the input does not violate `rule` in the first place
  Scenario scenario;
  Violation violation;  // first violation of `rule` in the shrunk scenario
  int attempts = 0;     // candidate simulations run
  long long steps_simulated = 0;  // total outer steps simulated while shrinking
  int accepted = 0;
  int size_before = 0, size_after = 0;
};

// A failure predicate: true when the scenario still fails (and fills the violation describing it).
using FailFn = std::function<bool(const Scenario&, Violation*)>;
ShrinkResult shrink(const Scenario& sc, const FailFn& fails, int max_attempts = 600);
ShrinkResult shrink(const Scenario& sc, const SimFn& sim, const std::string& rule, int max_attempts = 600);
inline ShrinkResult shrink(const Scenario& sc, BugMask bugs, const std::string& rule, int max_attempts = 600) {
  return shrink(sc, builtin_sim(bugs), rule, max_attempts);
}

}  // namespace rc
