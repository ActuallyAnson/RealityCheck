// Boundary-value scenario sampling (model-free): a random scenario with each numeric field pushed, with probability 0.3, to the most extreme value that still
// validates. Used by rc_extreme (the pre-declared test against plain random) and by the `boundary` option of the sample_random tool (agent v5).
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

#include "sim/scenario.hpp"
#include "util/rng.hpp"

namespace rc {

namespace detail {
inline void extreme_leaves(Json& j, std::vector<Json*>& out, const std::string& key) {
  if (j.is_object()) { std::vector<std::string> ks; for (auto& kv : j.obj()) ks.push_back(kv.first); for (auto& k : ks) extreme_leaves(j[k], out, k); }
  else if (j.is_array()) { for (Json& e : j.arr()) extreme_leaves(e, out, key); }
  else if (j.is_number() && key != "seed") out.push_back(&j);
}
inline bool extreme_valid(const Json& j, const std::string& domain) {
  try { return validate(scenario_from_json(j, domain)).empty(); } catch (const std::exception&) { return false; }
}
}  // namespace detail

// The ladder per field is x*{20, 5, 0.05, 0.2} then 0 and 1, tried from the farthest from x; the first value that validates is kept.
inline Scenario extreme_scenario(const Scenario& base, Rng& rng) {
  Json j = to_json(base);
  std::vector<Json*> ls;
  detail::extreme_leaves(j, ls, "");
  for (Json* l : ls) {
    if (rng.uni() >= 0.3) continue;
    const double x = l->num();
    const bool integral = x == std::floor(x);
    double cand[6] = {x * 20, x * 5, x * 0.05, x * 0.2, 0.0, 1.0};
    std::sort(cand, cand + 6, [&](double a, double b) { return std::fabs(a - x) > std::fabs(b - x); });
    for (double c : cand) {
      if (integral) c = std::round(c);
      if (c == x) continue;
      *l = c;
      if (detail::extreme_valid(j, base.domain)) break;
      *l = x;
    }
  }
  try { return scenario_from_json(j, base.domain); } catch (const std::exception&) { return base; }
}

}  // namespace rc
