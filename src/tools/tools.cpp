#include "tools/tools.hpp"

#include "sim/generic.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "shrink/shrink.hpp"
#include "util/rng.hpp"
#include "sim/domains.hpp"
#include "sim/extreme.hpp"
#include "sim/cartpole.hpp"
#include "sim/orbit.hpp"
#include "tools/render.hpp"

namespace rc {
namespace {

Json err(const std::string& m) {
  Json j = Json::object();
  j["ok"] = false;
  j["error"] = m;
  return j;
}

std::string hex(uint64_t h) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(h));
  return b;
}

Json body_json(const BodyState& q) {
  Json o = Json::object();
  o["x"] = q.x; o["y"] = q.y; o["vx"] = q.vx; o["vy"] = q.vy;
  return o;
}

}  // namespace

ToolSession::ToolSession(BugMask hidden_bugs, ToolBudget budget, std::string out_dir)
    : bugs_(hidden_bugs), sim_(builtin_sim(hidden_bugs)), budget_(budget), out_dir_(std::move(out_dir)) {}

ToolSession::ToolSession(SimFn sim, ToolBudget budget, std::string out_dir)
    : bugs_(), sim_(std::move(sim)), budget_(budget), out_dir_(std::move(out_dir)) {}

Json ToolSession::budget_json() const {
  Json b = Json::object();
  b["steps_used"] = static_cast<double>(usage_.steps_used);
  b["steps_remaining"] = static_cast<double>(std::max<long long>(0, budget_.max_steps - usage_.steps_used));
  b["tool_calls_remaining"] = std::max(0, budget_.max_tool_calls - usage_.tool_calls);
  return b;
}

std::string ToolSession::store(const Scenario& sc, Trace tr) {
  std::string id = "s" + hex(scenario_hash(sc)).substr(0, 10);
  if (!cache_.count(id)) {
    Entry e;
    e.sc = sc;
    e.viol = check_rules(sc, tr);
    e.tr = std::move(tr);
    cache_[id] = std::move(e);
  }
  return id;
}

Json ToolSession::failure_scenario_json(const std::string& id) const {
  auto it = cache_.find(id);
  return it == cache_.end() ? Json() : to_json(it->second.sc);
}

const ToolSession::Entry* ToolSession::find(const Json& a, std::string* e, std::string* id_out) {
  if (a.has("scenario_id") && a.at("scenario_id").is_string()) {
    auto it = cache_.find(a.at("scenario_id").str());
    if (it == cache_.end()) { *e = "unknown scenario_id; call run_scenario first"; return nullptr; }
    if (id_out) *id_out = it->first;
    return &it->second;
  }
  *e = "scenario_id is required";
  return nullptr;
}

Json ToolSession::call(const std::string& name, const Json& args) {
  if (usage_.tool_calls >= budget_.max_tool_calls) return err("tool-call budget exhausted");
  ++usage_.tool_calls;
  try {
    if (name == "run_scenario") return run_scenario(args);
    if (name == "sample_random") return sample_random(args);
    if (name == "check_rules") return check_rules_tool(args);
    if (name == "inspect_state") return inspect_state(args);
    if (name == "render_frames") return render_frames(args);
    if (name == "shrink") return shrink_tool(args);
    if (name == "replay") return replay(args);
  } catch (const std::exception& ex) {
    return err(std::string("bad arguments: ") + ex.what());
  }
  return err("unknown tool: " + name);
}

Json ToolSession::run_scenario(const Json& a) {
  if (!a.has("scenario")) return err("scenario is required");
  Scenario sc = scenario_from_json(a.at("scenario"), domain_);
  std::string why = validate(sc);
  if (!why.empty()) return err("invalid scenario: " + why);
  long long left = budget_.max_steps - usage_.steps_used;
  if (left <= 0) return err("step budget exhausted");
  bool truncated = false;
  if (sc.steps > left) { sc.steps = static_cast<int>(left); truncated = true; }
  Trace tr = sim_(sc);
  usage_.steps_used += sc.steps;
  ++usage_.scenarios_run;
  std::string id = store(sc, std::move(tr));
  const Entry& e = cache_.at(id);
  Json r = Json::object();
  r["ok"] = true;
  r["scenario_id"] = id;
  r["steps"] = sc.steps;
  r["truncated_to_budget"] = truncated;
  r["trace_hash"] = hex(e.tr.hash());
  r["violation_count"] = static_cast<int>(e.viol.size());
  Json rules = Json::array();
  for (const auto& v : e.viol) rules.push(v.rule);
  r["rules_violated"] = rules;
  if (!e.viol.empty()) {
    r["first_violation"] = to_json(e.viol.front());
    if (!has_failure_) {
      has_failure_ = true;
      failure_id_ = id;
      usage_.scenarios_until_first_violation = usage_.scenarios_run;
    }
  }
  const StepRecord &s0 = e.tr.rec.front(), &sn = e.tr.rec.back();
  r["energy_initial"] = s0.energy;
  r["energy_final"] = sn.energy;
  r["controller_work"] = sn.work;
  r["budget"] = budget_json();
  return r;
}

Json ToolSession::sample_random(const Json& a) {
  if (domain_ == "generic") return err("sample_random is not available for the generic domain");
  int count = std::max(1, std::min(400, static_cast<int>(a.num_or("count", 20))));
  const bool boundary = a.bool_or("boundary", false);
  int run = 0;
  for (int i = 0; i < count; ++i) {
    long long left = budget_.max_steps - usage_.steps_used;
    if (left <= 0) break;
    Scenario sc = random_scenario_for(domain_, sampler_.next());  // drawn from the session stream exactly as the random baseline does
    if (boundary && (i & 1)) { Rng er(sampler_.next()); sc = extreme_scenario(sc, er); }  // v5: every second scenario is pushed to boundary values
    bool truncated = false;
    if (sc.steps > left) { sc.steps = static_cast<int>(left); truncated = true; }
    Trace tr = sim_(sc);
    usage_.steps_used += sc.steps;
    ++usage_.scenarios_run;
    ++run;
    std::string id = store(sc, std::move(tr));
    const Entry& e = cache_.at(id);
    if (e.viol.empty()) continue;
    if (!has_failure_) { has_failure_ = true; failure_id_ = id; usage_.scenarios_until_first_violation = usage_.scenarios_run; }
    Json r = Json::object();
    r["ok"] = true;
    r["sampled"] = run;
    r["found_violation"] = true;
    r["scenario_id"] = id;
    r["steps"] = sc.steps;
    r["truncated_to_budget"] = truncated;
    r["trace_hash"] = hex(e.tr.hash());
    r["violation_count"] = static_cast<int>(e.viol.size());
    Json rules = Json::array();
    for (const auto& v : e.viol) rules.push(v.rule);
    r["rules_violated"] = rules;
    r["first_violation"] = to_json(e.viol.front());
    r["budget"] = budget_json();
    return r;
  }
  Json r = Json::object();
  r["ok"] = true;
  r["sampled"] = run;
  r["found_violation"] = false;
  r["note"] = "no declared rule was violated in any sampled scenario";
  r["budget"] = budget_json();
  return r;
}

Json ToolSession::check_rules_tool(const Json& a) {
  std::string e;
  const Entry* en = find(a, &e);
  if (!en) return err(e);
  Json r = Json::object();
  r["ok"] = true;
  Json v = Json::array();
  for (const auto& x : en->viol) v.push(to_json(x));
  r["violations"] = v;
  Json rules = Json::array();
  for (const auto& ri : rule_catalog(domain_)) {
    Json o = Json::object();
    o["id"] = ri.id;
    o["text"] = ri.text;
    rules.push(o);
  }
  r["rules"] = rules;
  return r;
}

Json ToolSession::inspect_state(const Json& a) {
  std::string e;
  const Entry* en = find(a, &e);
  if (!en) return err(e);
  int k = static_cast<int>(a.num_or("step", 0));
  if (k < 0 || k > en->tr.steps()) return err("step out of range 0.." + std::to_string(en->tr.steps()));
  const Scenario& sc = en->sc;
  const StepRecord& rec = en->tr.rec[static_cast<size_t>(k)];
  if (sc.domain == "orbit") return inspect_orbit(sc, rec, k);
  if (sc.domain == "cartpole") return inspect_cartpole(sc, rec, k);
  if (sc.domain == "traffic") return inspect_traffic(sc, rec, k);
  if (sc.domain == "generic") {
    Json g = Json::object();
    g["ok"] = true;
    g["step"] = k;
    Json sg = Json::object();
    const GenericModel& gm = generic_model();
    for (size_t i = 0; i < gm.signals.size() && i < rec.body.size(); ++i) sg[gm.signals[i].name] = rec.body[i].x;
    g["signals"] = sg;
    return g;
  }
  Json r = Json::object();
  r["ok"] = true;
  r["step"] = k;
  r["time"] = k * sc.dt;
  r["energy"] = rec.energy;
  r["controller_work"] = rec.work;
  r["controller_force"] = rec.fmag;
  r["substeps_in_step"] = rec.substeps;
  Json bodies = Json::array();
  for (size_t i = 0; i < rec.body.size(); ++i) {
    const BodyState& q = rec.body[i];
    const BodyDef& d = sc.bodies[i];
    Json o = body_json(q);
    o["index"] = static_cast<int>(i);
    o["r"] = d.r; o["m"] = d.m;
    o["speed"] = std::sqrt(q.vx * q.vx + q.vy * q.vy);
    o["arena_overshoot"] = std::fmax(std::fmax(d.r - q.x, q.x + d.r - sc.arena_w), std::fmax(d.r - q.y, q.y + d.r - sc.arena_h));
    Json walls = Json::array();
    for (size_t w = 0; w < sc.walls.size(); ++w) {
      const WallDef& wd = sc.walls[w];
      double cx = std::fmax(wd.x, std::fmin(q.x, wd.x + wd.w)), cy = std::fmax(wd.y, std::fmin(q.y, wd.y + wd.h));
      Json wo = Json::object();
      wo["wall"] = static_cast<int>(w);
      wo["distance_to_surface"] = std::hypot(q.x - cx, q.y - cy) - d.r;
      wo["centre_inside_box"] = q.x > wd.x && q.x < wd.x + wd.w && q.y > wd.y && q.y < wd.y + wd.h;
      walls.push(wo);
    }
    o["walls"] = walls;
    bodies.push(o);
  }
  r["bodies"] = bodies;
  Json pairs = Json::array();
  for (size_t i = 0; i < rec.body.size(); ++i)
    for (size_t j = i + 1; j < rec.body.size(); ++j) {
      double gap = std::hypot(rec.body[j].x - rec.body[i].x, rec.body[j].y - rec.body[i].y) - sc.bodies[i].r - sc.bodies[j].r;
      if (gap < 0.05) {
        Json po = Json::object();
        po["a"] = static_cast<int>(i); po["b"] = static_cast<int>(j); po["gap"] = gap;
        pairs.push(po);
      }
    }
  r["touching_pairs"] = pairs;
  return r;
}

Json ToolSession::inspect_traffic(const Scenario& sc, const StepRecord& rec, int k) {
  static const char* names[4] = {"N", "E", "S", "W"};
  static const char* sig[3] = {"red", "yellow", "green"};
  static const char* phase[6] = {"NS green", "NS yellow", "all-red before EW", "EW green", "EW yellow", "all-red before NS"};
  Json r = Json::object();
  r["ok"] = true;
  r["step"] = k;
  r["time_s"] = k;
  r["phase"] = phase[static_cast<int>(rec.fmag) % 6];
  Json ap = Json::array();
  for (int a = 0; a < 4; ++a) {
    const BodyState& q = rec.body[static_cast<size_t>(a)];
    Json o = Json::object();
    o["index"] = a;
    o["approach"] = names[a];
    o["signal"] = q.x;
    o["signal_name"] = sig[std::max(0, std::min(2, static_cast<int>(q.x)))];
    o["queue"] = q.y;
    o["wait"] = q.vx;
    o["flag"] = q.vy;
    ap.push(o);
  }
  r["approaches"] = ap;
  Json active = Json::array();
  for (const auto& e : sc.tr.events) {
    bool on = e.kind == 1 ? k == e.t : (k >= e.t && k < e.t + e.duration);
    if (!on) continue;
    Json eo = Json::object();
    eo["kind"] = e.kind == 0 ? "emergency" : e.kind == 1 ? "ped" : "detector_fail";
    eo["approach"] = names[e.approach];
    active.push(eo);
  }
  r["active_events"] = active;
  return r;
}

Json ToolSession::inspect_cartpole(const Scenario& sc, const StepRecord& rec, int k) {
  const BodyState& q = rec.body[0];
  Json r = Json::object();
  r["ok"] = true;
  r["step"] = k;
  r["time"] = k * sc.dt;
  r["x"] = q.x;
  r["v"] = q.vx;
  r["theta"] = q.y;
  r["omega"] = q.vy;
  r["controller_force"] = rec.fmag;
  r["force_limit"] = sc.cp.fmax;
  r["energy"] = rec.energy;
  r["external_work"] = rec.work;
  r["cart_to_track_end"] = 2.4 - std::fabs(q.x);
  r["pole_angle_margin"] = 0.8 - std::fabs(q.y);
  Json active = Json::array();
  for (size_t i = 0; i < sc.cp.pushes.size(); ++i) {
    const PushDef& p = sc.cp.pushes[i];
    double t = k * sc.dt;
    if (t >= p.t && t < p.t + p.len) {
      Json po = Json::object();
      po["push"] = static_cast<int>(i); po["force"] = p.force;
      active.push(po);
    }
  }
  r["active_pushes"] = active;
  return r;
}

Json ToolSession::inspect_orbit(const Scenario& sc, const StepRecord& rec, int k) {
  Json r = Json::object();
  r["ok"] = true;
  r["step"] = k;
  r["time"] = k * sc.dt;
  double ke, pe;
  orbit_energies(sc, rec, &ke, &pe);
  r["energy"] = rec.energy;
  r["kinetic"] = ke;
  r["potential"] = pe;
  r["momentum_x"] = rec.px;
  r["momentum_y"] = rec.py;
  r["angular_momentum"] = rec.ang;
  r["substeps_in_step"] = rec.substeps;
  Json bodies = Json::array();
  for (size_t i = 0; i < rec.body.size(); ++i) {
    const BodyState& q = rec.body[i];
    Json o = body_json(q);
    o["index"] = static_cast<int>(i);
    o["m"] = sc.bodies[i].m;
    o["speed"] = std::sqrt(q.vx * q.vx + q.vy * q.vy);
    o["radius_from_origin"] = std::hypot(q.x, q.y);
    bodies.push(o);
  }
  r["bodies"] = bodies;
  Json pairs = Json::array();
  for (size_t i = 0; i < rec.body.size(); ++i)
    for (size_t j = i + 1; j < rec.body.size(); ++j) {
      double d = std::hypot(rec.body[j].x - rec.body[i].x, rec.body[j].y - rec.body[i].y);
      if (d < 1.5) {
        Json po = Json::object();
        po["a"] = static_cast<int>(i); po["b"] = static_cast<int>(j); po["distance"] = d;
        pairs.push(po);
      }
    }
  r["close_pairs"] = pairs;
  return r;
}

Json ToolSession::render_frames(const Json& a) {
  std::string e, id;
  const Entry* en = find(a, &e, &id);
  if (!en) return err(e);
  if (en->sc.domain == "generic") return err("render_frames is not available for the generic domain; use inspect_state to read the signals");
  int from = static_cast<int>(a.num_or("from", 0)), to = static_cast<int>(a.num_or("to", en->tr.steps()));
  from = std::max(0, from); to = std::min(en->tr.steps(), to);
  if (from > to) return err("from must be <= to");
  int n = std::min(6, to - from + 1);
  std::vector<int> steps;
  for (int i = 0; i < n; ++i) steps.push_back(n == 1 ? from : from + (to - from) * i / (n - 1));
  int hl = static_cast<int>(a.num_or("highlight_body", -1));
  std::vector<uint8_t> png = render_strip_png(en->sc, en->tr, steps, hl);
  std::string dir = out_dir_.empty() ? "." : out_dir_;
  std::string path = dir + "/" + id + "_" + std::to_string(from) + "_" + std::to_string(to) + ".png";
  std::ofstream f(path, std::ios::binary);
  if (!f) return err("cannot write " + path);
  f.write(reinterpret_cast<const char*>(png.data()), static_cast<long>(png.size()));
  Json r = Json::object();
  r["ok"] = true;
  r["path"] = path;
  Json st = Json::array();
  for (int s : steps) st.push(s);
  r["steps_shown"] = st;
  r["png_bytes"] = static_cast<double>(png.size());
  return r;
}

Json ToolSession::shrink_tool(const Json& a) {
  std::string e, id;
  const Entry* en = find(a, &e, &id);
  if (!en) return err(e);
  if (en->viol.empty()) return err("scenario has no violation to shrink");
  std::string rule = a.str_or("rule", en->viol.front().rule);
  Scenario original = en->sc;  // copy: store() below may rehash the map
  ShrinkResult sr = rc::shrink(original, sim_, rule);
  usage_.shrink_steps += sr.steps_simulated;
  if (!sr.ok) return err("scenario does not violate rule " + rule);
  Trace tr = sim_(sr.scenario);
  std::string nid = store(sr.scenario, std::move(tr));
  shrunk_.insert(nid);
  Json r = Json::object();
  r["ok"] = true;
  r["scenario_id"] = nid;
  r["rule"] = rule;
  r["scenario"] = to_json(sr.scenario);
  r["violation"] = to_json(sr.violation);
  r["size_before"] = sr.size_before;
  r["size_after"] = sr.size_after;
  r["candidates_tried"] = sr.attempts;
  return r;
}

Json ToolSession::replay(const Json& a) {
  std::string e, id;
  const Entry* en = find(a, &e, &id);
  if (!en) return err(e);
  Trace again = sim_(en->sc);
  std::vector<Violation> v = check_rules(en->sc, again);
  bool same_hash = again.hash() == en->tr.hash();
  bool same_viol = v.size() == en->viol.size();
  for (size_t i = 0; same_viol && i < v.size(); ++i)
    same_viol = v[i].rule == en->viol[i].rule && v[i].step == en->viol[i].step && v[i].body == en->viol[i].body;
  Json r = Json::object();
  r["ok"] = true;
  r["scenario_id"] = id;
  r["trace_hash"] = hex(again.hash());
  r["identical_trace"] = same_hash;
  r["identical_violations"] = same_viol;
  r["violation_count"] = static_cast<int>(v.size());
  return r;
}

namespace {
Json prop(const char* type, const char* desc) {
  Json p = Json::object();
  p["type"] = type;
  p["description"] = desc;
  return p;
}
Json fn(const char* name, const char* desc, Json props, std::vector<std::string> required) {
  Json params = Json::object();
  params["type"] = "object";
  params["properties"] = props;
  Json req = Json::array();
  for (auto& r : required) req.push(r);
  params["required"] = req;
  Json f = Json::object();
  f["name"] = name;
  f["description"] = desc;
  f["parameters"] = params;
  return f;
}
}  // namespace

Json sample_random_spec(const std::string&, int version) {
  Json p = Json::object();
  p["count"] = prop("integer", version >= 4 ? "How many random scenarios to run (1 to 400, default 20). Stops at the first violation." : "How many random scenarios to run (1 to 100, default 20). Stops at the first violation.");
  if (version >= 5) p["boundary"] = prop("boolean", "If true, every second scenario is pushed to boundary values (each field, with probability 0.3, to the most extreme value the limits allow). Random scenarios rarely reach those corners.");
  return fn("sample_random", "Run random scenarios from the plain random generator (the same one an ordinary fuzzer uses). Each scenario spends the step budget exactly like run_scenario. Stops at the first violation and returns it like run_scenario does.", p, {});
}

Json tool_specs(const std::string& domain) {
  const bool orbit = domain == "orbit";
  const bool cart = domain == "cartpole";
  const bool traffic = domain == "traffic";
  const bool generic = domain == "generic";
  Json scen = prop("object",
                   generic ? generic_scenario_text().c_str() :
                   orbit ? "Scenario: {seed, dt, steps, G, bodies:[{x,y,vx,vy,m}]}. See the simulation description for limits."
                         : traffic ? "Scenario: {seed, steps, arrival_per_second:{N,E,S,W}, green_seconds:{ns,ew}, yellow_seconds, all_red_seconds, max_green_seconds, actuated, events:[{t,kind:emergency|ped|detector_fail,approach:N|E|S|W,duration}]}. See the simulation description for limits."
                         : cart ? "Scenario: {seed, dt, steps, plant:{cart_mass,pole_mass,pole_length,gravity}, initial:{x,v,theta,omega}, force_limit, sensor_noise, pushes:[{t,len,force}]}. See the simulation description for limits."
                         : "Scenario: {seed, dt, steps, gravity, bodies:[{x,y,vx,vy,r,m,e}], walls:[{x,y,w,h}], "
                           "controller:{enabled, body, kp, fmax, vcmd, noise, waypoints:[{x,y}]}}. See the simulation description for limits.");
  Json specs = Json::array();
  {
    Json p = Json::object();
    p["scenario"] = scen;
    specs.push(fn("run_scenario",
                  "Simulate a scenario and check every rule at every step. Costs steps from the hunt budget. "
                  "Returns scenario_id, whether any rule was violated and the first violation.",
                  p, {"scenario"}));
  }
  {
    Json p = Json::object();
    p["scenario_id"] = prop("string", "Id returned by run_scenario or shrink.");
    specs.push(fn("check_rules", "List every violation (earliest per rule and body) of an already-run scenario.", p, {"scenario_id"}));
  }
  {
    Json p = Json::object();
    p["scenario_id"] = prop("string", "Id returned by run_scenario or shrink.");
    p["step"] = prop("integer", "State index, 0 = initial state.");
    specs.push(fn("inspect_state", orbit ? "Exact state of every body at one step, with energies, momentum, angular momentum and close pairs." : traffic ? "Exact intersection state at one step: phase, each approach's signal, queue, red wait and flags, and the events active now." : cart ? "Exact cart-pole state at one step: cart position and velocity, pole angle and rate, controller force, margins to the limits." : "Exact state of every body at one step, with wall distances and touching pairs.", p, {"scenario_id", "step"}));
  }
  {
    Json p = Json::object();
    p["scenario_id"] = prop("string", "Id returned by run_scenario or shrink.");
    p["from"] = prop("integer", "First step.");
    p["to"] = prop("integer", "Last step; up to 6 evenly spaced frames are drawn.");
    p["highlight_body"] = prop("integer", "Body index drawn in red (optional).");
    specs.push(fn("render_frames", "Render a frame strip PNG for a step range.", p, {"scenario_id", "from", "to"}));
  }
  {
    Json p = Json::object();
    p["scenario_id"] = prop("string", "Id of a scenario that violates a rule.");
    p["rule"] = prop("string", "Rule id to preserve (defaults to the first violated rule).");
    specs.push(fn("shrink", "Reduce a failing scenario to a minimal one that still violates the same rule.", p, {"scenario_id"}));
  }
  {
    Json p = Json::object();
    p["scenario_id"] = prop("string", "Id returned by run_scenario or shrink.");
    specs.push(fn("replay", "Re-run a scenario from scratch and confirm the trace and violations are identical.", p, {"scenario_id"}));
  }
  return specs;
}

std::string simulation_description(const std::string& domain) {
  if (domain == "generic") return generic_description();
  if (domain == "traffic") {
    std::ostringstream o;
    o << "SIMULATION UNDER TEST\n"
         "A four-approach traffic-signal intersection (N, E, S, W). N and S form the NS direction, E and W the EW direction. One step is one second.\n"
         "- Vehicles arrive at random with a per-approach rate (seeded by `seed`) and queue; a green discharges one vehicle every two seconds.\n"
         "- The controller cycles NS green, NS yellow, all-red, EW green, EW yellow, all-red. A green lasts at least its green_seconds; when\n"
         "  `actuated` is true it ends early once its own detectors see no queue and the other direction has demand, and no later than\n"
         "  max_green_seconds. A direction that has waited long is recalled. With `actuated` false it is a fixed-time cycle.\n"
         "- Events: `emergency` (a vehicle on an approach: its direction gets and keeps green while active), `ped` (a pedestrian request: the next\n"
         "  all-red is extended by 8 s for a walk phase), `detector_fail` (the controller cannot see that approach's queue while active).\n\n"
         "SCENARIO LIMITS\n"
         "steps in [60, 3000]; arrival_per_second in [0, 0.4] per approach; green_seconds in [8, 45]; yellow_seconds in [3, 6]; all_red_seconds in [1, 4];\n"
         "max_green_seconds in [45, 90] and not below a green time; at most 6 events, of which at most 2 emergencies (duration 1 to 30) and 2 ped\n"
         "requests; detector_fail duration 1 to 220; event times inside the run.\n\n"
         "DECLARED RULES (checked at every second)\n";
    for (const auto& r : rule_catalog("traffic")) o << "- " << r.id << ": " << r.text << "\n";
    o << "\nThe rule checker, not you, decides what is a violation.\n";
    return o.str();
  }
  if (domain == "cartpole") {
    std::ostringstream o;
    o << "SIMULATION UNDER TEST\n"
         "A cart-pole balancing simulation: a cart moves along a frictionless track and a pole (point mass at its tip) is hinged on it.\n"
         "- A fixed state-feedback controller measures cart position and velocity, pole angle (with optional seeded sensor noise) and\n"
         "  angular rate, and applies a horizontal force u to the cart, clipped to the scenario's force_limit. Its gains adapt to the plant.\n"
         "- The plant is integrated with RK4; the controller acts once per outer step (dt). Pushes are extra external forces on the cart.\n"
         "- theta is the pole angle from upright in radians (0 = upright). A run is a pure function of the scenario (including `seed`).\n\n"
         "SCENARIO LIMITS\n"
         "dt in [0.002, 0.02]; steps in [1, 4000]; cart_mass in [0.5, 2]; pole_mass in [0.05, 0.3]; pole_length in [0.4, 1];\n"
         "gravity in [5, 12]; initial x in [-0.5, 0.5], v in [-0.5, 0.5], theta in [-0.1, 0.1], omega in [-0.3, 0.3];\n"
         "force_limit in [40, 80]; sensor_noise in [0, 0.02]; up to 3 pushes with t inside the run, len in [dt, 0.2], force in [-4, 4]; pushes that overlap in time add up and the total must stay within [-4, 4].\n\n"
         "DECLARED RULES (checked at every step)\n";
    for (const auto& r : rule_catalog("cartpole")) o << "- " << r.id << ": " << r.text << "\n";
    o << "\nThe rule checker, not you, decides what is a violation.\n";
    return o.str();
  }
  if (domain == "orbit") {
    std::ostringstream o;
    o << "SIMULATION UNDER TEST\n"
         "A deterministic 2D gravitational N-body simulation (x right, y up, no walls, no boundaries).\n"
         "- Every pair of bodies attracts with softened Newtonian gravity: acceleration of i from j is G m_j (r_j - r_i) / (|r_j - r_i|^2 + 0.3^2)^1.5.\n"
         "- Integration is a leapfrog scheme with fixed dt per outer step, split into substeps when bodies move fast or\n"
         "  accelerate strongly.\n"
         "- A run is a pure function of the scenario (including `seed`). Time starts at 0 and the unit of time is the step times dt.\n\n"
         "SCENARIO LIMITS\n"
         "dt in [1e-3, 1/30]; steps in [1, 4000]; G in [0.5, 5]; 2 to 6 bodies; m in [0.2, 20]; initial positions inside\n"
         "[0,20] x [0,12]; initial speed <= 10; any two bodies start at least 1.0 apart.\n\n"
         "DECLARED RULES (checked at every step)\n";
    for (const auto& r : rule_catalog("orbit")) o << "- " << r.id << ": " << r.text << "\n";
    o << "\nThe rule checker, not you, decides what is a violation.\n";
    return o.str();
  }
  std::ostringstream o;
  o << "SIMULATION UNDER TEST\n"
       "A deterministic 2D rigid-body world of circles in a 20 x 12 arena (x right, y up, floor at y=0).\n"
       "- Bodies are circles with radius r, mass m, restitution e (0..1). Gravity acts downward.\n"
       "- Static walls are axis-aligned boxes {x,y,w,h} (min corner and size). The arena has four boundary walls.\n"
       "- Integration is semi-implicit Euler with fixed dt, split into substeps when bodies move fast; collisions use\n"
       "  impulses plus positional correction.\n"
       "- One optional controller drives one body (a hover vehicle that ignores gravity) through up to 4 waypoints\n"
       "  with a velocity-error law, force limit fmax and optional seeded sensor noise.\n"
       "- A run is a pure function of the scenario (including `seed`).\n\n"
       "SCENARIO LIMITS\n"
       "dt in [1e-4, 1/30]; steps in [1, 4000]; gravity in [0, 30]; 1 to 8 bodies; 0 to 4 walls; r in [0.2, 1.2];\n"
       "m in [0.2, 20]; e in [0, 1]; initial speed <= 60; bodies must start inside the arena and not overlap each other\n"
       "or walls; wall thickness >= 0.1; controller kp in (0, 100], fmax in [5, 200], vcmd in (0, 20], noise in [0, 1],\n"
       "1 to 4 waypoints inside the arena.\n\n"
       "DECLARED RULES (checked at every step)\n";
  for (const auto& r : rule_catalog()) o << "- " << r.id << ": " << r.text << "\n";
  o << "\nThe rule checker, not you, decides what is a violation.\n";
  return o.str();
}

}  // namespace rc
