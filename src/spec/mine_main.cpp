// rc_mine: mine invariants from any engine's own runs, then check them on held-out scenarios. No model.
//   rc_mine --domain generic --engine card --pool 300 --heldout 1000 [--out mined.json]
//   rc_mine --domain orbit --engine 'exec:build/odeint_orbit rk4 0.02' --pool 300 --heldout 1000
// Reports how many held-out scenarios break a mined invariant (on a trusted simulator this is the false-positive rate; on a suspect one, the scenarios worth a look).
#include <cstdio>
#include <fstream>
#include <iostream>

#include "external/engine.hpp"
#include "rules/checker.hpp"
#include "sim/domains.hpp"
#include "spec/miner.hpp"

using namespace rc;

int main(int argc, char** argv) {
  std::string domain = "orbit", engine = "builtin", out;
  int pool_n = 300, held_n = 1000;
  uint64_t pool_seed = 900000, held_seed = 700000;
  MinerConfig cfg;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--domain") domain = val(); else if (a == "--engine") engine = val(); else if (a == "--pool") pool_n = std::atoi(val().c_str());
    else if (a == "--heldout") held_n = std::atoi(val().c_str()); else if (a == "--out") out = val();
    else if (a == "--bound") cfg.bound_margin = std::atof(val().c_str()); else if (a == "--drift") cfg.drift_factor = std::atof(val().c_str()); else if (a == "--step") cfg.step_factor = std::atof(val().c_str());
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  SimFn sim = make_engine(engine, domain);
  std::vector<Signals> pool;
  for (int i = 0; i < pool_n; ++i) { Scenario sc = random_scenario_for(domain, pool_seed + static_cast<uint64_t>(i)); pool.push_back(extract_signals(sc, sim(sc))); }
  std::vector<Invariant> mined = mine_invariants(pool, cfg);
  std::printf("mined %zu invariants from %d runs of %s\n", mined.size(), pool_n, engine.c_str());
  std::vector<int> fired(mined.size(), 0);
  int flagged = 0, flagged_and_rule = 0, rule_only = 0;
  for (int i = 0; i < held_n; ++i) {
    Scenario sc = random_scenario_for(domain, held_seed + static_cast<uint64_t>(i));
    Trace tr = sim(sc);
    Signals s = extract_signals(sc, tr);
    bool any = false;
    const bool rule = !check_rules(sc, tr).empty();
    for (size_t k = 0; k < mined.size(); ++k) if (check_invariant(mined[k], s).failed) { ++fired[k]; any = true; }
    flagged += any; flagged_and_rule += any && rule; rule_only += !any && rule;
  }
  std::printf("held-out scenarios that break a mined invariant: %d of %d\n", flagged, held_n);
  std::printf("of those, also violating a declared rule: %d; declared-rule violations the mined invariants did not flag: %d\n", flagged_and_rule, rule_only);
  for (size_t k = 0; k < mined.size(); ++k) if (fired[k]) std::printf("  %s: %d scenarios\n", mined[k].text.c_str(), fired[k]);
  if (!out.empty()) { Json arr = Json::array(); for (const Invariant& v : mined) arr.push(to_json(v)); std::ofstream(out) << arr.dump() << "\n"; }
  return 0;
}
