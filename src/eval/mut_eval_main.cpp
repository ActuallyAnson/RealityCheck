// mut_eval: evaluates ONE build of the simulators (clean, or linked against a mutated object file) on a fixed pool of random scenarios.
// One TSV line per scenario: index, steps, trace hash, hand-written rules violated, model-written invariants violated.
//   mut_eval --domain orbit --scenarios 1000 --base-seed 700000 [--invariants results/spec_orbit/spec.json]
#include <cstdio>
#include <iostream>

#include "eval/ground_truth.hpp"
#include "rules/checker.hpp"
#include "rules/metamorphic.hpp"
#include "sim/domains.hpp"
#include "spec/miner.hpp"
#include "spec/spec.hpp"

using namespace rc;

int main(int argc, char** argv) {
  std::string domain = "orbit", inv_path;
  int n = 1000, mr_n = 0, mine_n = 0;
  uint64_t mine_base = 900000;
  MinerConfig mcfg;
  bool calibrate = false;
  uint64_t base = 700000;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--domain") domain = val(); else if (a == "--scenarios") n = std::atoi(val().c_str());
    else if (a == "--base-seed") base = static_cast<uint64_t>(std::atoll(val().c_str())); else if (a == "--invariants") inv_path = val();
    else if (a == "--mine") mine_n = std::atoi(val().c_str()); else if (a == "--mine-seed") mine_base = static_cast<uint64_t>(std::atoll(val().c_str()));
    else if (a == "--mine-bound") mcfg.bound_margin = std::atof(val().c_str()); else if (a == "--mine-drift") mcfg.drift_factor = std::atof(val().c_str()); else if (a == "--mine-step") mcfg.step_factor = std::atof(val().c_str());
    else if (a == "--mr-scenarios") mr_n = std::atoi(val().c_str()); else if (a == "--calibrate-mr") calibrate = true;
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  std::vector<Invariant> invs;
  if (!inv_path.empty()) {
    Json r = Json::parse(read_file(inv_path));
    for (const Json& j : r.at("accepted").arr()) {
      std::vector<std::string> pr;
      auto v = parse_invariants("[" + j.dump() + "]", domain, &pr, r.num_or("language", 1) >= 2 ? 2 : 1);
      if (v.size() == 1) invs.push_back(v[0]);
    }
  }
  if (calibrate) {  // largest deviation of each relation on the (unmodified) simulator, to set tolerances
    MetamorphicDeviation mx; int pf = 0, rf = 0;
    SimFn sim = [](const Scenario& s) { return simulate(s, 0); };
    for (int i = 0; i < n; ++i) {
      MetamorphicDeviation d;
      metamorphic_failures(random_scenario_for(domain, base + static_cast<uint64_t>(i)), sim, &d);
      mx.perm = std::max(mx.perm, d.perm); mx.mirror = std::max(mx.mirror, d.mirror); mx.refine = std::max(mx.refine, d.refine); pf += !d.prefix_ok; rf += !d.repeat_ok;
    }
    std::printf("%s: prefix failures %d, repeat failures %d, max deviation perm %g mirror %g refine %g (-1 = never applied)\n", domain.c_str(), pf, rf, mx.perm, mx.mirror, mx.refine);
    return 0;
  }
  std::vector<Invariant> mined;  // mined from THIS build's own runs on a separate pool of scenarios (it never sees the unmodified simulator)
  if (mine_n > 0) {
    std::vector<Signals> pool;
    for (int i = 0; i < mine_n; ++i) { Scenario ms = random_scenario_for(domain, mine_base + static_cast<uint64_t>(i)); pool.push_back(extract_signals(ms, simulate(ms, 0))); }
    mined = mine_invariants(pool, mcfg);
  }
  for (int i = 0; i < n; ++i) {
    Scenario sc = random_scenario_for(domain, base + static_cast<uint64_t>(i));
    Trace tr = simulate(sc, 0);
    std::string rules, ids;
    for (const Violation& v : check_rules(sc, tr)) rules += (rules.empty() ? "" : ",") + v.rule;
    if (!invs.empty()) {
      Signals s = extract_signals(sc, tr);
      for (const Invariant& inv : invs) if (check_invariant(inv, s).failed) ids += (ids.empty() ? "" : ",") + inv.id;
    }
    std::string mn;
    if (!mined.empty()) {
      Signals s = extract_signals(sc, tr);
      for (const Invariant& inv : mined) if (check_invariant(inv, s).failed) { mn += (mn.empty() ? "" : ",") + inv.id; if (mn.size() > 60) break; }
    }
    std::string mr;
    if (i < mr_n) { SimFn sim = [](const Scenario& s2) { return simulate(s2, 0); }; for (const std::string& f : metamorphic_failures(sc, sim)) mr += (mr.empty() ? "" : ",") + f; }
    std::printf("%d\t%d\t%016llx\t%s\t%s\t%s\t%s\n", i, sc.steps, static_cast<unsigned long long>(tr.hash()), rules.empty() ? "-" : rules.c_str(), ids.empty() ? "-" : ids.c_str(), mr.empty() ? "-" : mr.c_str(), mn.empty() ? "-" : mn.c_str());
  }
  return 0;
}
