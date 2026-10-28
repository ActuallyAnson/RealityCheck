// Bring-your-own-simulator: runs an external program that speaks the RealityCheck JSON-lines protocol
// (docs/PROTOCOL.md) as a SimFn. One persistent child process; one request line in, one response line out.
#pragma once
#include <string>

#include "sim/world.hpp"

namespace rc {

// `command` is run through /bin/sh -c. `domain` ("rigid2d", "orbit" or "cartpole") fixes the scenario schema
// the program implements and how its reply is read. Throws std::runtime_error if the program misbehaves.
SimFn exec_sim(const std::string& command, const std::string& domain);

}  // namespace rc
