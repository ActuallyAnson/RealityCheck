// rc_hunt: random-scenario hunt (no model, no API) against any engine, with a shrunk reproduction and a
// ready-to-run regression-test bundle for the first failure of each rule.
//   rc_hunt --engine SPEC [--domain rigid2d|orbit|cartpole] [--scenarios N] [--seed S] [--out FILE] [--bundles DIR]
// SPEC: builtin | builtin:IDS | box2d | box2d-noccd | exec:COMMAND   (see src/external/engine.hpp, docs/PROTOCOL.md)
#include <sys/stat.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "baseline/random_baseline.hpp"
#include "external/bundle.hpp"
#include "external/engine.hpp"
#include "shrink/shrink.hpp"
#include "sim/domains.hpp"
#include "util/rng.hpp"

using namespace rc;

int main(int argc, char** argv) {
  int n = 2000;
  uint64_t seed = 1;
  std::string out, engine = "builtin", domain = "rigid2d", bundles;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
    if (a == "--scenarios") n = std::atoi(val().c_str());
    else if (a == "--seed") seed = std::strtoull(val().c_str(), nullptr, 10);
    else if (a == "--engine") engine = val();
    else if (a == "--domain") domain = val();
    else if (a == "--out") out = val();
    else if (a == "--bundles") bundles = val();
    else { std::cerr << "usage: rc_hunt --engine SPEC [--domain D] [--scenarios N] [--seed S] [--out FILE] [--bundles DIR]\n"; return 2; }
  }
  SimFn sim;
  try { sim = make_engine(engine, domain); } catch (const std::exception& e) { std::cerr << e.what() << "\n"; return 2; }
  Rng rng(seed);
  std::map<std::string, int> count, first_idx;
  std::map<std::string, Scenario> first;
  try {
    for (int i = 1; i <= n; ++i) {
      Scenario s = random_scenario_for(domain, rng.next());
      if (domain == "rigid2d") { s.ctrl.enabled = false; s.ctrl.waypoints.clear(); }  // external engines have no controller
      Trace tr = sim(s);
      std::map<std::string, bool> seen;
      for (const Violation& v : check_rules(s, tr)) {
        if (seen[v.rule]) continue;
        seen[v.rule] = true;
        if (count[v.rule]++ == 0) { first[v.rule] = s; first_idx[v.rule] = i; }
      }
    }
  } catch (const std::exception& e) { std::cerr << "engine error: " << e.what() << "\n"; return 2; }
  Json report = Json::object();
  report["engine"] = engine;
  report["domain"] = domain;
  report["scenarios"] = n;
  Json rules = Json::object();
  for (const auto& kv : count) {
    ShrinkResult sr = shrink(first[kv.first], sim, kv.first);
    Json j = Json::object();
    j["scenarios_flagged"] = kv.second;
    j["first_at_scenario"] = first_idx[kv.first];
    j["size_before"] = sr.size_before;
    j["size_after"] = sr.size_after;
    if (sr.ok) {
      j["shrunk"] = to_json(sr.scenario);
      j["violation"] = to_json(sr.violation);
      if (!bundles.empty()) {
        Json e = Json::object();
        e["domain"] = domain; e["rule"] = kv.first; e["first_violation_step"] = sr.violation.step; e["detail"] = sr.violation.detail;
        write_bundle(bundles + "/" + domain + "_" + kv.first, sr.scenario, e, "Found by rc_hunt random search on engine '" + engine + "'.");
      }
    }
    rules[kv.first] = j;
    std::cout << kv.first << ": " << kv.second << "/" << n << " scenarios, first at #" << first_idx[kv.first] << ", shrunk "
              << sr.size_before << " -> " << sr.size_after << "\n";
  }
  if (count.empty()) std::cout << "no rule violations in " << n << " scenarios\n";
  report["rules"] = rules;
  if (!out.empty()) std::ofstream(out) << report.dump() << "\n";
  return 0;
}
