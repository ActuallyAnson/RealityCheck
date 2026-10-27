// rc_box2d: run the random baseline (no model, no API) against unmodified Box2D and report what the
// declared rules flag, plus a shrunk reproduction of the first failure of each rule.
//   rc_box2d [--scenarios N] [--seed S] [--no-continuous] [--out FILE]
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "baseline/random_baseline.hpp"
#include "external/box2d_sim.hpp"
#include "shrink/shrink.hpp"
#include "util/rng.hpp"

using namespace rc;

int main(int argc, char** argv) {
  int n = 2000;
  uint64_t seed = 1;
  std::string out;
  Box2DOptions opt;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--scenarios" && i + 1 < argc) n = std::atoi(argv[++i]);
    else if (a == "--seed" && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 10);
    else if (a == "--no-continuous") opt.continuous = false;
    else if (a == "--out" && i + 1 < argc) out = argv[++i];
    else { std::cerr << "usage: rc_box2d [--scenarios N] [--seed S] [--no-continuous] [--out FILE]\n"; return 2; }
  }
  SimFn sim = box2d_sim(opt);
  Rng rng(seed);
  std::map<std::string, int> count;
  std::map<std::string, Scenario> first;
  std::map<std::string, int> first_idx;
  for (int i = 1; i <= n; ++i) {
    Scenario s = random_scenario(rng.next());
    s.ctrl.enabled = false;  // controllers are not part of Box2D
    s.ctrl.waypoints.clear();
    Trace tr = sim(s);
    std::map<std::string, bool> seen;
    for (const Violation& v : check_rules(s, tr)) {
      if (seen[v.rule]) continue;
      seen[v.rule] = true;
      if (count[v.rule]++ == 0) { first[v.rule] = s; first_idx[v.rule] = i; }
    }
  }
  Json report = Json::object();
  report["engine"] = "Box2D 3.x (unmodified)";
  report["continuous"] = opt.continuous;
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
    }
    rules[kv.first] = j;
    std::cout << kv.first << ": " << kv.second << "/" << n << " scenarios, first at #" << first_idx[kv.first]
              << ", shrunk " << sr.size_before << " -> " << sr.size_after << "\n";
  }
  if (count.empty()) std::cout << "no rule violations in " << n << " scenarios\n";
  report["rules"] = rules;
  if (!out.empty()) std::ofstream(out) << report.dump() << "\n";
  return 0;
}
