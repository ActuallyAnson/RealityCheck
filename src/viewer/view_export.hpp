// Converts runs into the viewer's domain-agnostic JSON: a static layer, per-entity kinds, flat per-step frames, named
// signal series and rule violations. The web viewer draws from this format only; a new simulator domain adds its own
// exporter and a renderer for its entity kinds. Nothing here reveals which seeded bugs were enabled.
#pragma once
#include <string>

#include "sim/bugs.hpp"
#include "sim/scenario.hpp"
#include "util/json.hpp"

namespace rc {

// Simulates the scenario under `mask` and exports it (domain "rigid2d").
Json view_rigid2d(const Scenario& sc, BugMask mask, const std::string& label);

// Builds the full viewer document for one saved run directory (events.jsonl, report.json, cost.jsonl): the first
// violating scenario the agent ran ("original"), the shrunk scenario from the report, the report, a compact event feed
// and cost totals. `mask` is only used to re-simulate the saved scenarios.
Json build_run_view(const std::string& run_dir, BugMask mask);

}  // namespace rc
