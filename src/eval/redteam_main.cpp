// rc_redteam: a deterministic adversary for the report validator (no model). For real failures found by random testing in the built-in domains, it builds a
// correct report, then applies forgeries and counts how many the validator accepts.
//   must-reject: the forged claim contradicts the recording (the validator should refuse all of these)
//   unchecked:   the forged claim is something the validator does not or cannot check (these quantify what a report's acceptance does NOT establish)
#include <cstdio>
#include <iostream>
#include <map>

#include "agent/agent.hpp"
#include "eval/ground_truth.hpp"
#include "sim/domains.hpp"
#include "tools/tools.hpp"

using namespace rc;

namespace {
Json report(const std::string& id, const std::string& rule, const std::string& text, const std::vector<std::pair<int, double>>& cites, bool with_value = true) {
  Json cs = Json::array();
  for (const auto& c : cites) {
    Json cit = Json::object();
    cit["step"] = c.first; cit["body"] = 0; cit["field"] = "energy"; if (with_value) cit["value"] = c.second; cit["claim"] = "total energy";
    cs.push(cit);
  }
  Json r = Json::object();
  r["scenario_id"] = id; r["rule"] = rule; r["explanation"] = text; r["citations"] = cs;
  return r;
}
}  // namespace

int main(int argc, char** argv) {
  int per_bug = 3;
  for (int i = 1; i + 1 < argc; i += 2) if (std::string(argv[i]) == "--per-bug") per_bug = std::atoi(argv[i + 1]);
  const std::vector<std::pair<std::string, std::vector<std::string>>> sets = {
      {"rigid2d", {"b01", "b03", "b04", "b05", "b07", "b08", "b09", "b10", "b11"}}, {"orbit", {"o01", "o02", "o03"}}, {"cartpole", {"c01", "c02", "c03"}}, {"traffic", {"t01", "t02", "t03"}}};
  struct Op { std::string name; bool must_reject; int n = 0, accepted = 0; };
  std::vector<Op> ops = {
      {"F1 value off by 5% + 1", true},        {"F2 value from a different step", true},   {"F3 value from a different failure", true}, {"F4 scenario id not the shrunk one", true},
      {"F5 rule that is not violated", true},  {"F6 step beyond the run", true},           {"F7 prose names a step that does not exist", true}, {"F8 never cites the violating step", true},
      {"U1 value within the 1% tolerance", false}, {"U2 citation with no value claimed", false}, {"U3 true numbers, false mechanism in prose", false}, {"U4 invented number in prose only", false}};
  int failures = 0;
  std::vector<std::pair<const ToolSession::Entry*, std::string>> pool_prev;  // for F3: a value from another failure
  double other_energy = 0; bool have_other = false;
  std::vector<std::unique_ptr<ToolSession>> keep;
  for (const auto& dom : sets) for (const std::string& bug : dom.second) {
    int idx = domain_bug_index(dom.first, bug);
    if (idx < 0) continue;
    int got = 0;
    for (uint64_t seed = 1; seed < 400 && got < per_bug; ++seed) {
      keep.push_back(std::make_unique<ToolSession>(static_cast<BugMask>(1) << idx, ToolBudget(), ""));
      ToolSession& ts = *keep.back();
      ts.set_domain(dom.first);
      Json a = Json::object(); a["scenario"] = to_json(random_scenario_for(dom.first, seed * 977));
      Json rs = ts.call("run_scenario", a);
      if (!rs.bool_or("ok", false) || !ts.has_failure()) continue;
      Json sa = Json::object(); sa["scenario_id"] = ts.failure_id();
      Json sh = ts.call("shrink", sa);
      if (!sh.bool_or("ok", false)) continue;
      const std::string id = sh.at("scenario_id").str();
      const ToolSession::Entry* run = ts.record(id);
      if (!run || run->viol.empty()) continue;
      const Violation& v = run->viol[0];
      const int steps = run->tr.steps();
      auto energy = [&](int k) { return run->tr.rec[static_cast<size_t>(k)].energy; };
      const std::string good = "At step " + std::to_string(v.step) + " the rule " + v.rule + " fails; the total energy there is recorded in the citation.";
      if (!validate_report(*run, true, report(id, v.rule, good, {{v.step, energy(v.step)}})).ok) continue;  // honest baseline must pass, else skip this failure
      ++failures; ++got;
      auto run_op = [&](size_t i, bool accepted) { ++ops[i].n; ops[i].accepted += accepted; };
      auto accepted = [&](const Json& r, bool shrunk = true) { return validate_report(*run, shrunk, r).ok; };
      run_op(0, accepted(report(id, v.rule, good, {{v.step, energy(v.step) * 1.05 + 1.0}})));
      { int s2 = (v.step + steps / 2 + 1) % (steps + 1); double w = energy(s2); if (std::fabs(w - energy(v.step)) > std::max(0.02, 0.01 * std::fabs(w)) * 2) run_op(1, accepted(report(id, v.rule, good, {{v.step, w}}))); }
      if (have_other && std::fabs(other_energy - energy(v.step)) > std::max(0.02, 0.01 * std::fabs(energy(v.step))) * 2) run_op(2, accepted(report(id, v.rule, good, {{v.step, other_energy}})));
      other_energy = energy(v.step); have_other = true;
      run_op(3, accepted(report(id, v.rule, good, {{v.step, energy(v.step)}}), false));
      { std::string other; for (const RuleInfo& ri : rule_catalog(dom.first)) { bool viol = false; for (const auto& vv : run->viol) viol |= vv.rule == ri.id; if (!viol) { other = ri.id; break; } }
        if (!other.empty()) run_op(4, accepted(report(id, other, good, {{v.step, energy(v.step)}}))); }
      run_op(5, accepted(report(id, v.rule, good, {{steps + 5, 1.0}})));
      run_op(6, accepted(report(id, v.rule, "At step " + std::to_string(steps + 40) + " the rule " + v.rule + " fails badly.", {{v.step, energy(v.step)}})));
      if (v.step > 0) run_op(7, accepted(report(id, v.rule, good, {{0, energy(0)}})));
      run_op(8, accepted(report(id, v.rule, good, {{v.step, energy(v.step) * 1.005}})));
      run_op(9, accepted(report(id, v.rule, good, {{v.step, 0}}, false)));
      run_op(10, accepted(report(id, v.rule, "At step " + std::to_string(v.step) + " the rule " + v.rule + " fails because a hash table overflowed in the logging subsystem, which has nothing to do with the physics.", {{v.step, energy(v.step)}})));
      run_op(11, accepted(report(id, v.rule, "At step " + std::to_string(v.step) + " the rule " + v.rule + " fails; the peak velocity reached 123456 units per second before this.", {{v.step, energy(v.step)}})));
    }
  }
  std::printf("%d real failures (random testing, built-in domains), honest report accepted in each\n\n| forgery | kind | forged reports | accepted by the validator |\n|---|---|---|---|\n", failures);
  int bad = 0;
  for (const Op& o : ops) { std::printf("| %s | %s | %d | %d |\n", o.name.c_str(), o.must_reject ? "must reject" : "not checkable", o.n, o.accepted); if (o.must_reject) bad += o.accepted; }
  std::printf("\nmust-reject forgeries accepted: %d\n", bad);
  return bad == 0 ? 0 : 1;
}
