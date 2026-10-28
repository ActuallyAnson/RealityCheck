// Domain registry helpers: scenario sampling and seeded-bug ids by domain name.
#pragma once
#include <string>

#include "baseline/random_baseline.hpp"
#include "sim/cartpole.hpp"
#include "sim/generic.hpp"
#include "sim/orbit.hpp"
#include "sim/traffic.hpp"

namespace rc {

inline Scenario random_scenario_for(const std::string& domain, uint64_t seed) {
  if (domain == "orbit") return random_orbit_scenario(seed);
  if (domain == "cartpole") return random_cartpole_scenario(seed);
  if (domain == "traffic") return random_traffic_scenario(seed);
  if (domain == "generic") return random_generic_scenario(seed);
  return random_scenario(seed);
}

inline int domain_num_bugs(const std::string& domain) {
  if (domain == "orbit") return kNumOrbitBugs;
  if (domain == "cartpole") return kNumCartBugs;
  if (domain == "traffic") return kNumTrafficBugs;
  if (domain == "generic") return 0;
  return kNumBugs;
}

inline std::string domain_bug_name(const std::string& domain, int i) {
  if (domain == "orbit") return orbit_bug_id(i);
  if (domain == "cartpole") return cart_bug_id(i);
  if (domain == "traffic") return traffic_bug_id(i);
  return bug_id(i);
}

}  // namespace rc
