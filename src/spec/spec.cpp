#include "spec/spec.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

#include "tools/tools.hpp"

namespace rc {

const std::vector<SignalInfo>& signal_catalog(const std::string& domain) {
  static const std::vector<SignalInfo> orbit = {
      {"energy", "total kinetic plus potential energy of all bodies"},
      {"angular_momentum", "total angular momentum about the origin"},
      {"momentum_x", "total linear momentum, x component"},
      {"momentum_y", "total linear momentum, y component"},
      {"max_speed", "largest body speed"},
      {"min_pair_distance", "smallest distance between any two bodies"},
      {"max_radius", "largest body distance from the origin"}};
  static const std::vector<SignalInfo> cart = {
      {"cart_x", "cart position"}, {"cart_v", "cart velocity"}, {"theta", "pole angle from upright, radians"},
      {"omega", "pole angular rate"}, {"controller_force", "magnitude of the largest controller force applied during the step"},
      {"force_limit", "the scenario's controller force limit (constant)"}, {"energy", "total mechanical energy of cart and pole"}};
  static const std::vector<SignalInfo> traffic = {
      {"state_N", "signal on approach N: 0 red, 1 yellow, 2 green"}, {"state_E", "signal on approach E: 0 red, 1 yellow, 2 green"},
      {"state_S", "signal on approach S: 0 red, 1 yellow, 2 green"}, {"state_W", "signal on approach W: 0 red, 1 yellow, 2 green"},
      {"ns_active", "max of state_N and state_S"}, {"ew_active", "max of state_E and state_W"},
      {"queue_N", "vehicles queued on N"}, {"queue_E", "vehicles queued on E"}, {"queue_S", "vehicles queued on S"}, {"queue_W", "vehicles queued on W"},
      {"longest_red_wait", "seconds the longest-waiting approach has been red"}};
  static const std::vector<SignalInfo> rigid = {
      {"energy", "total kinetic plus potential energy of all bodies"}, {"max_speed", "largest body speed"},
      {"arena_overshoot", "largest distance any body's edge is outside the arena (0 when all inside)"},
      {"min_pair_gap", "smallest gap between two bodies' edges (negative = overlapping)"},
      {"controller_force", "magnitude of the largest controller force this step (0 if no controller)"},
      {"force_limit", "the controller's force limit (constant; 0 if no controller)"}};
  if (domain == "orbit") return orbit;
  if (domain == "cartpole") return cart;
  if (domain == "traffic") return traffic;
  return rigid;
}

Signals extract_signals(const Scenario& sc, const Trace& tr) {
  Signals s;
  const std::string& d = sc.domain;
  for (const StepRecord& r : tr.rec) {
    if (d == "traffic") {
      static const char* nm[4] = {"N", "E", "S", "W"};
      double st[4], wmax = 0;
      for (int a = 0; a < 4; ++a) {
        const BodyState& b = r.body[static_cast<size_t>(a)];
        st[a] = b.x;
        s[std::string("state_") + nm[a]].push_back(b.x);
        s[std::string("queue_") + nm[a]].push_back(b.y);
        wmax = std::max(wmax, b.vx);
      }
      s["ns_active"].push_back(std::max(st[0], st[2]));
      s["ew_active"].push_back(std::max(st[1], st[3]));
      s["longest_red_wait"].push_back(wmax);
    } else if (d == "cartpole") {
      const BodyState& q = r.body[0];
      s["cart_x"].push_back(q.x); s["theta"].push_back(q.y); s["cart_v"].push_back(q.vx); s["omega"].push_back(q.vy);
      s["controller_force"].push_back(r.fmag); s["force_limit"].push_back(sc.cp.fmax); s["energy"].push_back(r.energy);
    } else {
      double ms = 0, mind = 1e300, maxr = 0, over = 0, gap = 1e300;
      for (size_t i = 0; i < r.body.size(); ++i) {
        const BodyState& b = r.body[i];
        ms = std::max(ms, std::hypot(b.vx, b.vy));
        maxr = std::max(maxr, std::hypot(b.x, b.y));
        double rad = i < sc.bodies.size() ? sc.bodies[i].r : 0;
        over = std::max({over, rad - b.x, b.x + rad - sc.arena_w, rad - b.y, b.y + rad - sc.arena_h});
        for (size_t j = i + 1; j < r.body.size(); ++j) {
          double dist = std::hypot(b.x - r.body[j].x, b.y - r.body[j].y), rj = j < sc.bodies.size() ? sc.bodies[j].r : 0;
          mind = std::min(mind, dist); gap = std::min(gap, dist - rad - rj);
        }
      }
      if (r.body.size() < 2) { mind = 0; gap = 1e9; }
      s["energy"].push_back(r.energy); s["max_speed"].push_back(ms);
      if (d == "orbit") {
        s["angular_momentum"].push_back(r.ang); s["momentum_x"].push_back(r.px); s["momentum_y"].push_back(r.py);
        s["min_pair_distance"].push_back(mind); s["max_radius"].push_back(maxr);
      } else {
        s["arena_overshoot"].push_back(over); s["min_pair_gap"].push_back(gap);
        s["controller_force"].push_back(r.fmag); s["force_limit"].push_back(sc.ctrl.enabled ? sc.ctrl.fmax : 0);
      }
    }
  }
  return s;
}

namespace {
bool known_signal(const std::string& domain, const std::string& n) {
  for (const SignalInfo& si : signal_catalog(domain)) if (si.name == n) return true;
  return false;
}
}  // namespace

std::vector<Invariant> parse_invariants(const std::string& text, const std::string& domain, std::vector<std::string>* problems) {
  std::vector<Invariant> out;
  auto bad = [&](const std::string& m) { if (problems) problems->push_back(m); };
  size_t a = text.find('['), b = text.rfind(']');
  if (a == std::string::npos || b == std::string::npos || b < a) { bad("no JSON array found in the answer"); return out; }
  Json arr;
  try { arr = Json::parse(text.substr(a, b - a + 1)); } catch (const std::exception& e) { bad(std::string("answer is not valid JSON: ") + e.what()); return out; }
  if (!arr.is_array()) { bad("answer is not an array"); return out; }
  std::set<std::string> ids;
  int k = 0;
  for (const Json& j : arr.arr()) {
    ++k;
    std::string tag = "item " + std::to_string(k) + ": ";
    try {
      Invariant v;
      v.id = j.str_or("id", "inv" + std::to_string(k));
      v.kind = j.str_or("kind", "");
      v.text = j.str_or("text", "");
      v.signal = j.str_or("signal", "");
      if (!ids.insert(v.id).second) { bad(tag + "duplicate id " + v.id); continue; }
      if (v.kind != "bound" && v.kind != "conserved" && v.kind != "monotone" && v.kind != "exclusive") { bad(tag + "unknown kind"); continue; }
      if (!known_signal(domain, v.signal)) { bad(tag + "unknown signal '" + v.signal + "'"); continue; }
      v.abs_tol = j.num_or("abs_tol", 0); v.rel_tol = j.num_or("rel_tol", 0); v.add = j.num_or("add", 0);
      if (v.abs_tol < 0 || v.rel_tol < 0) { bad(tag + "negative tolerance"); continue; }
      if (v.kind == "bound") {
        v.op = j.str_or("op", "");
        if (v.op != "<=" && v.op != ">=") { bad(tag + "op must be <= or >="); continue; }
        v.rhs_signal = j.str_or("rhs_signal", "");
        v.has_value = j.has("value") && j.at("value").is_number(); v.value = j.num_or("value", 0);
        v.has_mult = j.has("mult") && j.at("mult").is_number(); v.mult = j.num_or("mult", 0);
        int forms = (v.has_value ? 1 : 0) + (!v.rhs_signal.empty() ? 1 : 0) + (v.has_mult ? 1 : 0);
        if (forms != 1) { bad(tag + "bound needs exactly one of value, rhs_signal, mult"); continue; }
        if (!v.rhs_signal.empty() && !known_signal(domain, v.rhs_signal)) { bad(tag + "unknown rhs_signal"); continue; }
      } else if (v.kind == "monotone") {
        v.direction = j.str_or("direction", "");
        if (v.direction != "nonincreasing" && v.direction != "nondecreasing") { bad(tag + "direction must be nonincreasing or nondecreasing"); continue; }
      } else if (v.kind == "exclusive") {
        v.signal_b = j.str_or("signal_b", "");
        if (!known_signal(domain, v.signal_b)) { bad(tag + "unknown signal_b"); continue; }
      }
      out.push_back(v);
    } catch (const std::exception& e) { bad(tag + e.what()); }
  }
  return out;
}

Json to_json(const Invariant& v) {
  Json j = Json::object();
  j["id"] = v.id; j["kind"] = v.kind; j["signal"] = v.signal; j["text"] = v.text;
  if (v.kind == "bound") {
    j["op"] = v.op;
    if (v.has_value) j["value"] = v.value;
    if (!v.rhs_signal.empty()) j["rhs_signal"] = v.rhs_signal;
    if (v.has_mult) j["mult"] = v.mult;
    if (v.add != 0) j["add"] = v.add;
  } else if (v.kind == "conserved") { j["abs_tol"] = v.abs_tol; j["rel_tol"] = v.rel_tol; }
  else if (v.kind == "monotone") { j["direction"] = v.direction; j["abs_tol"] = v.abs_tol; }
  else j["signal_b"] = v.signal_b;
  return j;
}

InvFail check_invariant(const Invariant& v, const Signals& s) {
  InvFail f;
  auto it = s.find(v.signal);
  if (it == s.end() || it->second.empty()) return f;
  const std::vector<double>& x = it->second;
  const int n = static_cast<int>(x.size());
  auto fail = [&](int k, double obs, double lim) { f.failed = true; f.step = k; f.observed = obs; f.limit = lim; };
  for (int k = 0; k < n; ++k) {
    if (!std::isfinite(x[k])) { fail(k, x[k], 0); return f; }
    if (v.kind == "bound") {
      double rhs = v.has_value ? v.value : !v.rhs_signal.empty() ? s.at(v.rhs_signal)[k] + v.add : v.mult * x[0] + v.add;
      if (v.op == "<=" ? x[k] > rhs : x[k] < rhs) { fail(k, x[k], rhs); return f; }
    } else if (v.kind == "conserved") {
      double lim = v.abs_tol + v.rel_tol * std::fabs(x[0]);
      if (std::fabs(x[k] - x[0]) > lim) { fail(k, x[k], x[0]); return f; }
    } else if (v.kind == "monotone" && k > 0) {
      double d = x[k] - x[k - 1];
      if (v.direction == "nonincreasing" ? d > v.abs_tol : d < -v.abs_tol) { fail(k, x[k], x[k - 1]); return f; }
    } else if (v.kind == "exclusive") {
      const std::vector<double>& y = s.at(v.signal_b);
      if (x[k] > 0.5 && y[k] > 0.5) { fail(k, x[k], y[k]); return f; }
    }
  }
  return f;
}

std::string spec_prompt(const std::string& domain) {
  std::string desc = simulation_description(domain);
  size_t cut = desc.find("DECLARED RULES");
  if (cut != std::string::npos) desc = desc.substr(0, cut);
  std::ostringstream o;
  o << "You are writing the correctness invariants for a simulator, using only its description. Nobody has told you how it is implemented or whether it has defects.\n\n"
    << desc << "\nSIGNALS you may refer to (recorded at every step, step 0 = initial state):\n";
  for (const SignalInfo& si : signal_catalog(domain)) o << "- " << si.name << ": " << si.text << "\n";
  o << "\nINVARIANT LANGUAGE: a JSON array of objects, each with `id`, `text` (one sentence of why this must hold) and a `kind`:\n"
       "- {\"kind\":\"bound\",\"signal\":S,\"op\":\"<=\"|\">=\", and exactly one of \"value\":number | \"rhs_signal\":T (compare with signal T at the same step, optional \"add\") | \"mult\":m (compare with m * S at step 0, optional \"add\")}\n"
       "- {\"kind\":\"conserved\",\"signal\":S,\"abs_tol\":a,\"rel_tol\":r}   |S(t) - S(0)| <= a + r*|S(0)| at every step\n"
       "- {\"kind\":\"monotone\",\"signal\":S,\"direction\":\"nonincreasing\"|\"nondecreasing\",\"abs_tol\":a}   step-to-step change allowed within a\n"
       "- {\"kind\":\"exclusive\",\"signal\":S,\"signal_b\":T}   S and T are never both greater than 0.5\n\n"
       "TASK: propose up to 12 invariants that must hold for EVERY valid scenario within the limits above, if the simulator is correct. The engine will run each one on many\n"
       "clean simulations and discard any that ever fails, so do not guess tight numbers you cannot justify; but an invariant that is too loose catches nothing. Prefer\n"
       "conservation laws, bounds implied by the description, and safety properties. Answer with the JSON array only.";
  return o.str();
}

std::string revision_prompt(const std::vector<Invariant>& rejected, const std::vector<std::string>& evidence) {
  std::ostringstream o;
  o << "The engine ran your invariants on many clean simulations. These FAILED on a valid run, so they are not invariants of this simulator as specified (or your tolerances were too tight):\n";
  for (size_t i = 0; i < rejected.size(); ++i) o << "- " << to_json(rejected[i]).dump() << "  -> " << evidence[i] << "\n";
  o << "\nReturn a JSON array with revised versions of the ones worth keeping (looser tolerance, different signal, or a different form). Do not repeat the ones that passed. Answer with the JSON array only; an empty array is fine.";
  return o.str();
}

}  // namespace rc
