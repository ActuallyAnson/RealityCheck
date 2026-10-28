#include "external/engine.hpp"

#include <sstream>
#include <stdexcept>

#include "eval/ground_truth.hpp"
#include "external/exec_sim.hpp"
#include "sim/generic.hpp"
#ifdef RC_HAVE_BOX2D
#include "external/box2d_sim.hpp"
#endif

namespace rc {

SimFn make_engine(const std::string& spec, const std::string& domain) {
  if (spec.empty() || spec == "builtin") return builtin_sim(0);
  if (spec.rfind("builtin:", 0) == 0) {
    BugMask mask = 0;
    std::stringstream ss(spec.substr(8));
    std::string tok;
    while (std::getline(ss, tok, ',')) {
      int idx = domain_bug_index(domain, tok);
      if (idx < 0) throw std::runtime_error("unknown bug id for domain " + domain + ": " + tok);
      mask |= bug_bit(idx);
    }
    return builtin_sim(mask);
  }
  if (spec.rfind("exec:", 0) == 0) return exec_sim(spec.substr(5), domain);
  if (spec == "card") {  // the command named in the model card (generic domain)
    if (domain != "generic") throw std::runtime_error("engine \"card\" is for the generic domain");
    if (generic_model().command.empty()) throw std::runtime_error("the model card has no \"command\"");
    return exec_sim(generic_model().command, domain);
  }
#ifdef RC_HAVE_BOX2D
  if (spec == "box2d" || spec == "box2d-noccd") {
    if (domain != "rigid2d") throw std::runtime_error("Box2D only implements the rigid2d domain");
    Box2DOptions bo;
    bo.continuous = spec == "box2d";
    SimFn inner = box2d_sim(bo);
    return [inner](const Scenario& s) { Scenario c = s; c.ctrl.enabled = false; return inner(c); };  // Box2D has no controller
  }
#endif
  throw std::runtime_error("unknown or unavailable engine '" + spec + "'");
}

}  // namespace rc
