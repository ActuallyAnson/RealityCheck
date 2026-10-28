// mut_eval: evaluates ONE build of the simulators (clean, or linked against a mutated object file) on a fixed pool of random scenarios.
// One TSV line per scenario: index, steps, trace hash, hand-written rules violated, model-written invariants violated.
//   mut_eval --domain orbit --scenarios 1000 --base-seed 700000 [--invariants results/spec_orbit/spec.json]
#include <cstdio>
#include <iostream>

#include "eval/ground_truth.hpp"
#include "rules/checker.hpp"
#include "sim/domains.hpp"
#include "spec/spec.hpp"

using namespace rc;

int main(int argc, char** argv) {
  std::string domain = "orbit", inv_path;
  int n = 1000;
  uint64_t base = 700000;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--domain") domain = val(); else if (a == "--scenarios") n = std::atoi(val().c_str());
    else if (a == "--base-seed") base = static_cast<uint64_t>(std::atoll(val().c_str())); else if (a == "--invariants") inv_path = val();
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
  for (int i = 0; i < n; ++i) {
    Scenario sc = random_scenario_for(domain, base + static_cast<uint64_t>(i));
    Trace tr = simulate(sc, 0);
    std::string rules, ids;
    for (const Violation& v : check_rules(sc, tr)) rules += (rules.empty() ? "" : ",") + v.rule;
    if (!invs.empty()) {
      Signals s = extract_signals(sc, tr);
      for (const Invariant& inv : invs) if (check_invariant(inv, s).failed) ids += (ids.empty() ? "" : ",") + inv.id;
    }
    std::printf("%d\t%d\t%016llx\t%s\t%s\n", i, sc.steps, static_cast<unsigned long long>(tr.hash()), rules.empty() ? "-" : rules.c_str(), ids.empty() ? "-" : ids.c_str());
  }
  return 0;
}
