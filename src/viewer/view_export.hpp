// Converts runs into the viewer's domain-agnostic JSON: a static layer, per-entity kinds, flat per-step frames, named
// signal series and rule violations. The web viewer draws from this format only; a new simulator domain adds its own
// exporter and a renderer for its entity kinds. Nothing here reveals which seeded bugs were enabled.
#pragma once
#include <string>
#include <vector>

#include "sim/bugs.hpp"
#include "sim/scenario.hpp"
#include "util/json.hpp"

namespace rc {

// Simulates the scenario under `mask` and exports it (domain "rigid2d").
Json view_rigid2d(const Scenario& sc, BugMask mask, const std::string& label);

// Builds the full viewer document for one saved run directory (events.jsonl, report.json, cost.jsonl): the first
// violating scenario the agent ran ("original"), the shrunk scenario from the report, the report, a compact event feed
// and cost totals. `mask` is only used to re-simulate the saved scenarios.
Json build_run_view(const std::string& run_dir, BugMask mask, const std::string& domain = "rigid2d");

// Turns raw agent events (events.jsonl lines) into the viewer's compact feed events, one at a time. For live runs it
// also emits {"kind":"view","which":"original"|"shrunk","view":...} messages the moment the first violating scenario
// and the shrunk scenario exist, so the canvases can start playing while the agent is still working.
class LiveFeed {
public:
  explicit LiveFeed(BugMask mask, std::string domain = "rigid2d") : mask_(mask), domain_(std::move(domain)) {}
  // Returns the messages to send for this raw event (first is always {"kind":"event", ...}).
  std::vector<Json> push(const Json& raw);
  // The scenario of the first violating run_scenario seen so far (null until then).
  const Json& first_failing() const { return first_failing_; }
  const Json& first_run() const { return first_run_; }
private:
  BugMask mask_;
  std::string domain_;
  Json last_args_, first_failing_, first_run_;
  bool original_sent_ = false, shrunk_sent_ = false;
};

}  // namespace rc
