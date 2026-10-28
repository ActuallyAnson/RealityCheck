// Generic domain: test any program that emits named per-step signals, without writing a C++ domain.
// The simulator is described by a JSON "model card" (see docs/GENERIC_DOMAIN.md): parameters with ranges, signal names, rules written in
// the invariant language of src/spec, and a command that speaks the exec protocol. The card is loaded once, from `set_generic_model` or
// from the file named by the RC_MODEL_CARD environment variable. Everything else (tools, shrinker, agent, API, spec-to-rules) is unchanged.
#pragma once
#include <string>
#include <vector>

#include "rules/checker.hpp"
#include "sim/scenario.hpp"
#include "sim/world.hpp"
#include "spec/spec.hpp"
#include "util/json.hpp"

namespace rc {

struct GenericParam {
  std::string name, type;  // type: float | int | choice
  double min = 0, max = 1, def = 0;
  bool log_scale = false;
  std::vector<std::string> choices;
  std::string text;
};
struct GenericRule {
  Invariant inv;
  std::string id, text;
};
struct GenericModel {
  std::string name, description, command;
  std::vector<GenericParam> params;
  int steps_min = 10, steps_max = 1000, steps_def = 200;
  std::vector<SignalInfo> signals;
  std::vector<GenericRule> rules;
};

GenericModel parse_generic_model(const Json& card);       // throws std::runtime_error with a precise message
void set_generic_model(const GenericModel& m);
const GenericModel& generic_model();                      // loads $RC_MODEL_CARD on first use; throws if none

std::string generic_validate(const Scenario& s);
Json generic_to_json(const Scenario& s);
Scenario generic_from_json(const Json& j);
Scenario random_generic_scenario(uint64_t seed);
std::vector<Violation> check_generic_rules(const Scenario& sc, const Trace& tr);
std::string generic_scenario_text();                      // scenario format and limits, for the tool schema and prompt
std::string generic_description();                        // simulation description (no rules), for prompts
const std::vector<RuleInfo>& generic_rule_catalog();
// Trace <-> signals: signal i is stored in body i's x; step 0 is the initial state.
Signals generic_signals(const Scenario& sc, const Trace& tr);

}  // namespace rc
