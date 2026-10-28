// The six deterministic tools offered to a tester (the model or a script).
// A ToolSession owns the hidden bug mask: tools only ever return what the simulation did, never which
// bugs were enabled.
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include "rules/checker.hpp"
#include "sim/bugs.hpp"
#include "sim/scenario.hpp"
#include "sim/world.hpp"
#include "util/json.hpp"

namespace rc {

struct ToolBudget {
  long long max_steps = 300000;  // hunt budget: outer steps simulated by run_scenario
  int max_tool_calls = 200;
};

struct ToolUsage {
  long long steps_used = 0;      // run_scenario steps (counts against the hunt budget)
  long long shrink_steps = 0;    // steps simulated by shrink (reported separately)
  int scenarios_run = 0;
  int tool_calls = 0;
  int scenarios_until_first_violation = 0;  // 1-based; 0 if none yet
};

class ToolSession {
public:
  ToolSession(BugMask hidden_bugs, ToolBudget budget, std::string out_dir = "");
  // Test any other simulator through the same tools (it is called as a pure function of the scenario).
  ToolSession(SimFn sim, ToolBudget budget, std::string out_dir = "");

  // name in {run_scenario, check_rules, inspect_state, render_frames, shrink, replay}.
  // Always returns an object with "ok"; on failure {"ok":false,"error":...}.
  Json call(const std::string& name, const Json& args);

  // Domain of the scenarios this session accepts ("rigid2d" default or "orbit").
  void set_domain(const std::string& d) { domain_ = d; }
  const std::string& domain() const { return domain_; }
  const ToolUsage& usage() const { return usage_; }
  const ToolBudget& budget() const { return budget_; }
  bool budget_exhausted() const { return usage_.steps_used >= budget_.max_steps || usage_.tool_calls >= budget_.max_tool_calls; }

  // First violation found by run_scenario in this session (the scenario that was run, untruncated copy).
  bool has_failure() const { return has_failure_; }
  const std::string& failure_id() const { return failure_id_; }
  Json failure_scenario_json(const std::string& id) const;  // scenario as stored for an id (null if unknown)

  struct Entry {
    Scenario sc;
    Trace tr;
    std::vector<Violation> viol;
  };
  const Entry* record(const std::string& id) const {
    auto it = cache_.find(id);
    return it == cache_.end() ? nullptr : &it->second;
  }
  bool is_shrunk(const std::string& id) const { return shrunk_.count(id) > 0; }

private:
  BugMask bugs_;
  SimFn sim_;
  std::string domain_ = "rigid2d";
  ToolBudget budget_;
  std::string out_dir_;
  ToolUsage usage_;
  std::map<std::string, Entry> cache_;
  std::set<std::string> shrunk_;
  bool has_failure_ = false;
  std::string failure_id_;

  Json run_scenario(const Json& a);
  Json check_rules_tool(const Json& a);
  Json inspect_state(const Json& a);
  Json inspect_orbit(const Scenario& sc, const StepRecord& rec, int k);
  Json render_frames(const Json& a);
  Json shrink_tool(const Json& a);
  Json replay(const Json& a);
  const Entry* find(const Json& a, std::string* err, std::string* id_out = nullptr);
  std::string store(const Scenario& sc, Trace tr);
  Json budget_json() const;
};

// Tool schemas (OpenAI function-calling style: name, description, parameters) for the model client.
Json tool_specs(const std::string& domain = "rigid2d");
// Plain-language description of the simulation, rules and scenario limits. Contains no bug information.
std::string simulation_description(const std::string& domain = "rigid2d");

}  // namespace rc
