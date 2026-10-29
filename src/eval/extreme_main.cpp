// rc_extreme: offline (no model) test of boundary-value sampling against plain random sampling at the same step budget.
// The "extreme" arm draws a random scenario and then pushes each numeric field, with probability 0.3, to the most extreme value that still validates
// (the ladder is x*{0.05, 0.2, 5, 20} then 0 and 1; the first valid one in order of distance from x is kept). Half of the scenarios in that arm are untouched
// random ones, so it is random testing plus boundary cases, not a replacement. Declared before running (docs/PREREG_FINAL.md addendum): 40 fresh seeds,
// 300000 steps, one setting, no tuning.
//   rc_extreme --domain rigid2d --bugs b06,b09 --seeds 40 --seed-base 8000 [--budget-steps 300000]
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

#include "eval/ground_truth.hpp"
#include "rules/checker.hpp"
#include "sim/domains.hpp"
#include "sim/extreme.hpp"
#include "util/rng.hpp"

using namespace rc;

int main(int argc, char** argv) {
  std::string domain = "rigid2d", bugs_arg;
  int seeds = 40;
  uint64_t seed_base = 8000;
  long long budget = 300000;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--domain") domain = val(); else if (a == "--bugs") bugs_arg = val(); else if (a == "--seeds") seeds = std::atoi(val().c_str());
    else if (a == "--seed-base") seed_base = std::strtoull(val().c_str(), nullptr, 10); else if (a == "--budget-steps") budget = std::atoll(val().c_str());
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  std::stringstream ss(bugs_arg);
  std::string tok;
  std::cout << "| bug | random found | extreme-mix found |\n|---|---|---|\n";
  while (std::getline(ss, tok, ',')) {
    int idx = domain_bug_index(domain, tok);
    if (idx < 0) { std::cerr << "unknown bug " << tok << "\n"; return 2; }
    BugMask mask = bug_bit(idx);
    int found_arm[2] = {0, 0};
    for (int arm = 0; arm < 2; ++arm) {
      for (int s = 0; s < seeds; ++s) {
        Rng rng(seed_base + static_cast<uint64_t>(s));
        long long used = 0;
        bool found = false;
        int n = 0;
        while (used < budget && !found) {
          Scenario sc = random_scenario_for(domain, rng.next());
          if (arm == 1 && (n++ & 1)) sc = extreme_scenario(sc, rng);
          long long left = budget - used;
          if (sc.steps > left) sc.steps = static_cast<int>(left);
          Trace tr = simulate(sc, mask);
          used += sc.steps;
          if (!check_rules(sc, tr).empty() && check_rules(sc, simulate(sc, 0)).empty()) found = true;
        }
        found_arm[arm] += found;
      }
    }
    std::cout << "| " << tok << " | " << found_arm[0] << "/" << seeds << " | " << found_arm[1] << "/" << seeds << " |\n";
    std::cout.flush();
  }
  return 0;
}
