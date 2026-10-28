// rc_orbit_cal: calibration of a non-default domain with the random baseline only (no model, no API).
//   rc_orbit_cal [--domain orbit|cartpole] [--scenarios N] [--seed S] [--clean-only] [--clean-only]
// Prints, for the clean simulator and for each seeded orbit bug, how many random scenarios are flagged
// and by which rule. The clean row must be all zeros before any bug is trusted.
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

#include "rules/checker.hpp"
#include "sim/cartpole.hpp"
#include "sim/orbit.hpp"
#include "util/rng.hpp"

using namespace rc;

int main(int argc, char** argv) {
  int n = 5000;
  uint64_t seed = 1;
  bool clean_only = false;
  std::string domain = "orbit";
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--scenarios" && i + 1 < argc) n = std::atoi(argv[++i]);
    else if (a == "--seed" && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 10);
    else if (a == "--clean-only") clean_only = true;
    else if (a == "--domain" && i + 1 < argc) domain = argv[++i];
    else { std::cerr << "usage: rc_orbit_cal [--scenarios N] [--seed S] [--clean-only]\n"; return 2; }
  }
  const bool cart = domain == "cartpole";
  const int nbugs = cart ? kNumCartBugs : kNumOrbitBugs;
  for (int row = -1; row < (clean_only ? 0 : nbugs); ++row) {
    BugMask mask = row < 0 ? 0 : (1u << row);
    Rng rng(seed);
    std::map<std::string, int> by_rule;
    int flagged = 0, first = 0;
    for (int i = 1; i <= n; ++i) {
      Scenario s = cart ? random_cartpole_scenario(rng.next()) : random_orbit_scenario(rng.next());
      auto v = check_rules(s, simulate(s, mask));
      if (!v.empty()) { ++flagged; if (!first) first = i; for (auto& x : v) ++by_rule[x.rule]; }
    }
    std::cout << (row < 0 ? "clean" : cart ? cart_bug_id(row) : orbit_bug_id(row)) << ": flagged " << flagged << "/" << n << " (first #" << first << ")";
    for (auto& kv : by_rule) std::cout << "  " << kv.first << "=" << kv.second;
    std::cout << "\n";
  }
  return 0;
}
