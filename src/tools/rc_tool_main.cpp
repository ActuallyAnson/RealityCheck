// Developer CLI: runs the tools against a simulation with chosen bugs.
//   rc_tool [--bugs b01,b05] [--out DIR] [--budget STEPS] < script.jsonl
// Each stdin line is {"tool": "...", "args": {...}}; one result JSON line is printed per call.
// Bugs are chosen by the operator (the harness side); a tester would only ever see the tool results.
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "tools/tools.hpp"

using namespace rc;

int main(int argc, char** argv) {
  BugMask mask = 0;
  std::string out = ".";
  ToolBudget budget;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--bugs" && i + 1 < argc) {
      std::stringstream ss(argv[++i]);
      std::string tok;
      while (std::getline(ss, tok, ',')) {
        int idx = bug_index(tok);
        if (idx < 0) { std::cerr << "unknown bug id: " << tok << "\n"; return 2; }
        mask |= bug_bit(idx);
      }
    } else if (a == "--out" && i + 1 < argc) out = argv[++i];
    else if (a == "--budget" && i + 1 < argc) budget.max_steps = std::atoll(argv[++i]);
    else if (a == "--describe") { std::cout << simulation_description() << tool_specs().dump() << "\n"; return 0; }
    else { std::cerr << "usage: rc_tool [--bugs b01,b05] [--out DIR] [--budget STEPS] [--describe]\n"; return 2; }
  }
  ToolSession session(mask, budget, out);
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) continue;
    try {
      Json req = Json::parse(line);
      std::cout << session.call(req.at("tool").str(), req.has("args") ? req.at("args") : Json::object()).dump() << "\n";
    } catch (const std::exception& e) {
      std::cout << "{\"ok\":false,\"error\":\"" << e.what() << "\"}\n";
    }
  }
  return 0;
}
