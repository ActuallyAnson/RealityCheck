// The tester agent: a tool-calling loop around a ModelBackend. The model chooses where to look; the
// deterministic ToolSession decides what is a violation. Explanations must cite real steps and states.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "model/model.hpp"
#include "tools/tools.hpp"

namespace rc {

struct Citation {
  int step = 0;
  int body = -1;
  std::string field;       // x | y | vx | vy | speed | energy (optional)
  bool has_value = false;
  double value = 0;
  std::string claim;
};

struct CitationCheck {
  bool ok = false;
  std::vector<std::string> problems;
};

// Checks a submitted report against the recorded run. Pure function of (session record, report JSON).
CitationCheck validate_report(const ToolSession::Entry& run, bool shrunk, const Json& report);

struct AgentConfig {
  std::string model;                 // overrides the backend default when non-empty
  bool native_tools = true;          // false: strict JSON action protocol
  int max_turns = 40;
  int max_report_attempts = 3;
  int max_tokens = 4096;
  std::string reasoning_effort;      // "" = omit
  std::string out_dir = ".";
  size_t tool_result_chars = 6000;
};

struct AgentResult {
  bool found_failure = false;        // a violation was confirmed by the engine in this session
  bool report_submitted = false;
  bool report_valid = false;         // citations validated against the recorded states
  int report_attempts = 0, rejected_reports = 0;
  std::string stop_reason;           // report_accepted | report_rejected | turns | tool_budget | model_budget | model_error | no_tool_calls
  int turns = 0;
  std::string scenario_id, rule;
  Json minimal_scenario;
  Json violation;
  std::string explanation;
  std::vector<Citation> citations;
  std::string frame_strip_path;
  std::string vision_note;           // what the model saw in the rendered frames
  bool vision_agrees = false;
  bool vision_done = false;
};

using EventSink = std::function<void(const Json&)>;

std::string system_prompt(bool native_tools);
AgentResult run_agent(BudgetedModel& model, ToolSession& tools, const AgentConfig& cfg, const EventSink& sink = nullptr);
Json agent_result_to_json(const AgentResult& r);
// Writes report.json and trace.json (replayable state history for the web viewer) into dir.
void write_report_files(const AgentResult& r, const ToolSession& tools, const std::string& dir);

}  // namespace rc
