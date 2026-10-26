#include "tools/tools.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "shrink/shrink.hpp"
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
    : bugs_(hidden_bugs), budget_(budget), out_dir_(std::move(out_dir)) {}

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
  Scenario sc = scenario_from_json(a.at("scenario"));
  std::string why = validate(sc);
  if (!why.empty()) return err("invalid scenario: " + why);
  long long left = budget_.max_steps - usage_.steps_used;
  if (left <= 0) return err("step budget exhausted");
  bool truncated = false;
  if (sc.steps > left) { sc.steps = static_cast<int>(left); truncated = true; }
  Trace tr = simulate(sc, bugs_);
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
  for (const auto& ri : rule_catalog()) {
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

Json ToolSession::render_frames(const Json& a) {
  std::string e, id;
  const Entry* en = find(a, &e, &id);
  if (!en) return err(e);
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
  ShrinkResult sr = rc::shrink(original, bugs_, rule);
  usage_.shrink_steps += sr.steps_simulated;
  if (!sr.ok) return err("scenario does not violate rule " + rule);
  Trace tr = simulate(sr.scenario, bugs_);
  std::string nid = store(sr.scenario, std::move(tr));
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
  Trace again = simulate(en->sc, bugs_);
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

Json tool_specs() {
  Json scen = prop("object",
                   "Scenario: {seed, dt, steps, gravity, bodies:[{x,y,vx,vy,r,m,e}], walls:[{x,y,w,h}], "
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
    specs.push(fn("inspect_state", "Exact state of every body at one step, with wall distances and touching pairs.", p, {"scenario_id", "step"}));
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

std::string simulation_description() {
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
