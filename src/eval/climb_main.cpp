// rc_climb: offline (no model) comparison of random scenario sampling with a margin-guided hill climber at the same step
// budget. The climber starts from a random scenario, mutates one numeric field at a time within the public scenario limits
// (invalid mutations are discarded for free), and keeps a mutation when it does not lower the largest rule margin
// (rule_margins: measured / allowed over the run). It is the control for any "model + guided search" claim.
//   rc_climb --domain rigid2d --bugs b12 --seeds 10 [--budget-steps 300000] [--iters 25]
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

#include "eval/ground_truth.hpp"
#include "rules/checker.hpp"
#include "sim/domains.hpp"
#include "util/rng.hpp"

using namespace rc;

namespace {
double gauss(Rng& r) { return std::sqrt(-2 * std::log(std::max(1e-12, r.uni()))) * std::cos(6.283185307179586 * r.uni()); }

void leaves(Json& j, std::vector<Json*>& out, const std::string& key) {
  if (j.is_object()) { std::vector<std::string> ks; for (auto& kv : j.obj()) ks.push_back(kv.first); for (auto& k : ks) leaves(j[k], out, k); }
  else if (j.is_array()) { for (Json& e : j.arr()) leaves(e, out, key); }
  else if ((j.is_number() || j.is_bool()) && key != "seed") out.push_back(&j);
}

// One random valid mutation of `sc` (up to 30 tries); returns false if none was valid.
bool mutate(const Scenario& sc, Rng& rng, Scenario* out) {
  for (int t = 0; t < 30; ++t) {
    Json j = to_json(sc);
    std::vector<Json*> ls;
    leaves(j, ls, "");
    if (ls.empty()) return false;
    Json* l = ls[static_cast<size_t>(rng.irange(0, static_cast<int>(ls.size()) - 1))];
    if (l->is_bool()) { if (rng.uni() < 0.3) *l = !l->boolean(); else continue; }
    else {
      double x = l->num(), nx;
      bool integral = x == std::floor(x);
      if (rng.uni() < 0.5) nx = x * std::exp(0.3 * gauss(rng)); else nx = x + gauss(rng) * (0.5 * std::fabs(x) + 0.1);
      if (integral) nx = std::round(nx) == x ? x + (rng.uni() < 0.5 ? -1 : 1) : std::round(nx);
      *l = nx;
    }
    try {
      Scenario m = scenario_from_json(j, sc.domain);
      if (validate(m).empty()) { *out = m; return true; }
    } catch (const std::exception&) {}
  }
  return false;
}

double objective(const Scenario& sc, const Trace& tr) {
  double best = 0;
  for (auto& kv : rule_margins(sc, tr)) best = std::max(best, kv.second);
  return best;
}
}  // namespace

int main(int argc, char** argv) {
  std::string domain = "rigid2d", bugs_arg;
  int seeds = 10, iters = 25;
  long long budget = 300000;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--domain") domain = val(); else if (a == "--bugs") bugs_arg = val(); else if (a == "--seeds") seeds = std::atoi(val().c_str());
    else if (a == "--iters") iters = std::atoi(val().c_str()); else if (a == "--budget-steps") budget = std::atoll(val().c_str());
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  std::stringstream ss(bugs_arg);
  std::string tok;
  std::cout << "| bug | arm | found (attributable) | median simulations to first | median steps to first |\n|---|---|---|---|---|\n";
  while (std::getline(ss, tok, ',')) {
    int idx = domain_bug_index(domain, tok);
    if (idx < 0) { std::cerr << "unknown bug " << tok << "\n"; return 2; }
    BugMask mask = bug_bit(idx);
    for (int arm = 0; arm < 2; ++arm) {
      std::vector<long long> sims_to, steps_to;
      for (int s = 0; s < seeds; ++s) {
        Rng rng(7000 + static_cast<uint64_t>(s));
        long long used = 0, sims = 0;
        bool found = false;
        auto try_sc = [&](Scenario& sc, double* obj) {  // simulate, charge, test; true when an attributable violation appears
          long long left = budget - used;
          if (left <= 0) return true;
          if (sc.steps > left) sc.steps = static_cast<int>(left);
          Trace tr = simulate(sc, mask);
          used += sc.steps; ++sims;
          if (obj) *obj = objective(sc, tr);
          if (!check_rules(sc, tr).empty() && check_rules(sc, simulate(sc, 0)).empty()) { found = true; return true; }
          return false;
        };
        while (used < budget && !found) {
          Scenario cur = random_scenario_for(domain, rng.next());
          double cobj = 0;
          if (try_sc(cur, &cobj) || arm == 0) continue;
          for (int it = 0; it < iters && used < budget && !found; ++it) {
            Scenario nx;
            if (!mutate(cur, rng, &nx)) break;
            double nobj = 0;
            if (try_sc(nx, &nobj)) break;
            if (nobj >= cobj) { cur = nx; cobj = nobj; }
          }
        }
        if (found) { sims_to.push_back(sims); steps_to.push_back(used); }
      }
      auto med = [](std::vector<long long> v) { if (v.empty()) return std::string("-"); std::sort(v.begin(), v.end()); return std::to_string(v[v.size() / 2]); };
      std::cout << "| " << tok << " | " << (arm ? "margin hill-climb" : "random") << " | " << sims_to.size() << "/" << seeds << " | " << med(sims_to) << " | " << med(steps_to) << " |\n";
      std::cout.flush();
    }
  }
  return 0;
}
