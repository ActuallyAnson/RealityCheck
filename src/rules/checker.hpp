// Declared invariants, checked at every step. The checker is the ground truth for "violation".
#pragma once
#include <string>
#include <vector>

#include "sim/scenario.hpp"
#include "sim/world.hpp"
#include "util/json.hpp"

namespace rc {

struct RuleSpec {
  double pen_tol = 0.15;       // allowed wall penetration / arena overshoot (length units)
  double energy_rel = 0.10;    // energy may exceed E0 + controller work by this fraction of the scale ...
  double energy_abs = 5.0;     // ... plus this absolute amount
  double force_tol = 1e-6;     // relative slack on the controller force limit
};

struct Violation {
  std::string rule;  // no_nan | in_arena | no_wall_penetration | no_tunneling | energy_bound | controller_force_limit
  int step = 0;      // first step at which the rule fails (state index, 0 = initial)
  int body = -1;
  double value = 0;  // measured quantity
  double limit = 0;  // allowed quantity
  std::string detail;
};

struct RuleInfo {
  const char* id;
  const char* text;
};
const std::vector<RuleInfo>& rule_catalog(const std::string& domain = "rigid2d");

// Returns at most one violation per (rule, body): the earliest one. Sorted by step, then rule id.
std::vector<Violation> check_rules(const Scenario& sc, const Trace& tr, const RuleSpec& spec = RuleSpec());

std::vector<Violation> check_orbit_rules(const Scenario& sc, const Trace& tr);
std::vector<Violation> check_cartpole_rules(const Scenario& sc, const Trace& tr);

Json to_json(const Violation& v);

}  // namespace rc
