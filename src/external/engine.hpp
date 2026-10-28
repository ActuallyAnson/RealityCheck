// Engine selection shared by the command-line tools: which simulator a scenario is run through.
//   builtin            our simulator, no bugs (the clean reference)
//   builtin:b02,b05    our simulator with those seeded bugs (ids of the domain; harness use only)
//   box2d, box2d-noccd unmodified Box2D 3.x (rigid2d domain; only when built with Box2D)
//   exec:<command>     any program speaking the JSON-lines protocol of docs/PROTOCOL.md
#pragma once
#include <string>

#include "sim/world.hpp"

namespace rc {

// Throws std::runtime_error for an unknown or unavailable engine.
SimFn make_engine(const std::string& spec, const std::string& domain);

}  // namespace rc
