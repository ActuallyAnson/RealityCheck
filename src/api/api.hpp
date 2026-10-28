// The deterministic RealityCheck tools as a service: one pure function from (operation, JSON arguments) to a JSON result.
// No model is involved and no key is needed. The HTTP server and the MCP stdio server in api_main.cpp are thin wrappers around it.
#pragma once
#include <string>

#include "util/json.hpp"

namespace rc {

struct ApiOptions {
  bool allow_exec = false;  // exec:<command> engines run an arbitrary program; refused unless the operator turns this on
};

// Operations: domains, random_scenario, run, shrink, bundle, hunt. Always returns an object with "ok";
// on failure {"ok":false,"error":...}. Never throws.
Json api_call(const std::string& op, const Json& args, const ApiOptions& opt = ApiOptions());

// MCP tool descriptors (name, description, inputSchema) for the same operations, prefixed rc_.
Json mcp_tool_list();

}  // namespace rc
