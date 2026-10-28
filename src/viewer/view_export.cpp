#include "viewer/view_export.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

#include "rules/checker.hpp"
#include "sim/world.hpp"

namespace rc {
namespace {

double r3(double x) { return std::isfinite(x) ? std::round(x * 1000.0) / 1000.0 : 0.0; }

std::string slurp(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return "";
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

std::string clip(const std::string& s, size_t n) { return s.size() <= n ? s : s.substr(0, n) + "...";}

}  // namespace

namespace {
Json view_orbit(const Scenario& sc, BugMask mask, const std::string& label) {
  Trace tr = simulate(sc, mask);
  std::vector<Violation> viol = check_rules(sc, tr);
  Json v = Json::object();
  v["label"] = label;
  v["domain"] = "orbit";
  Json world = Json::object();
  world["w"] = 20; world["h"] = 12;
  v["world"] = world;
  v["dt"] = sc.dt;
  v["steps"] = tr.steps();
  v["static"] = Json::array();
  Json ents = Json::array();
  for (size_t i = 0; i < sc.bodies.size(); ++i) {
    Json o = Json::object();
    o["id"] = "body " + std::to_string(i);
    o["k"] = "circle";  // frame layout: x, y, vx, vy; drawn with a radius that grows with mass
    o["r"] = 0.15 + 0.1 * std::cbrt(sc.bodies[i].m);
    o["m"] = sc.bodies[i].m;
    o["ctrl"] = false;
    ents.push(o);
  }
  v["ents"] = ents;
  Json frames = Json::array(), energy = Json::array(), ang = Json::array(), px = Json::array(), py = Json::array();
  for (const StepRecord& rec : tr.rec) {
    Json f = Json::array();
    for (const BodyState& b : rec.body) { f.push(r3(b.x)); f.push(r3(b.y)); f.push(r3(b.vx)); f.push(r3(b.vy)); }
    frames.push(f);
    energy.push(r3(rec.energy)); ang.push(r3(rec.ang)); px.push(r3(rec.px)); py.push(r3(rec.py));
  }
  v["frames"] = frames;
  Json sig = Json::object();
  sig["energy"] = energy;
  sig["angular_momentum"] = ang;
  sig["momentum_x"] = px;
  sig["momentum_y"] = py;
  v["signals"] = sig;
  v["signal_limits"] = Json::object();
  Json vj = Json::array();
  for (const Violation& x : viol) vj.push(to_json(x));
  v["violations"] = vj;
  v["scenario"] = to_json(sc);
  v["size"] = scenario_size(sc);
  return v;
}
}  // namespace

Json view_cartpole(const Scenario& sc, BugMask mask, const std::string& label) {
  Trace tr = simulate(sc, mask);
  std::vector<Violation> viol = check_rules(sc, tr);
  Json v = Json::object();
  v["label"] = label;
  v["domain"] = "cartpole";
  Json world = Json::object();
  world["w"] = 6; world["h"] = 2.6; world["x0"] = -3;
  v["world"] = world;
  v["dt"] = sc.dt;
  v["steps"] = tr.steps();
  Json stat = Json::array();
  for (double e : {-2.4, 2.4}) {
    Json o = Json::object();
    o["k"] = "track_end"; o["x"] = e;
    stat.push(o);
  }
  v["static"] = stat;
  Json ents = Json::array();
  Json o = Json::object();
  o["id"] = "cart-pole";
  o["k"] = "cartpole";  // frame layout: x, theta, v, omega
  o["r"] = sc.cp.l;
  o["m"] = sc.cp.mp;
  ents.push(o);
  v["ents"] = ents;
  Json frames = Json::array(), energy = Json::array(), force = Json::array();
  for (const StepRecord& rec : tr.rec) {
    const BodyState& q = rec.body[0];
    Json f = Json::array();
    f.push(r3(q.x)); f.push(r3(q.y)); f.push(r3(q.vx)); f.push(r3(q.vy));
    frames.push(f);
    energy.push(r3(rec.energy)); force.push(r3(rec.fmag));
  }
  v["frames"] = frames;
  Json sig = Json::object();
  sig["energy"] = energy;
  sig["controller_force"] = force;
  v["signals"] = sig;
  Json lim = Json::object();
  lim["controller_force"] = sc.cp.fmax;
  v["signal_limits"] = lim;
  Json vj = Json::array();
  for (const Violation& x : viol) vj.push(to_json(x));
  v["violations"] = vj;
  v["scenario"] = to_json(sc);
  v["size"] = scenario_size(sc);
  return v;
}

Json view_rigid2d(const Scenario& sc, BugMask mask, const std::string& label) {
  if (sc.domain == "orbit") return view_orbit(sc, mask, label);
  if (sc.domain == "cartpole") return view_cartpole(sc, mask, label);
  Trace tr = simulate(sc, mask);
  std::vector<Violation> viol = check_rules(sc, tr);
  Json v = Json::object();
  v["label"] = label;
  v["domain"] = "rigid2d";
  Json world = Json::object();
  world["w"] = sc.arena_w;
  world["h"] = sc.arena_h;
  v["world"] = world;
  v["dt"] = sc.dt;
  v["steps"] = tr.steps();
  Json stat = Json::array();
  for (const WallDef& w : sc.walls) {
    Json o = Json::object();
    o["k"] = "rect"; o["x"] = w.x; o["y"] = w.y; o["w"] = w.w; o["h"] = w.h;
    stat.push(o);
  }
  if (sc.ctrl.enabled) {
    int i = 0;
    for (const Waypoint& p : sc.ctrl.waypoints) {
      Json o = Json::object();
      o["k"] = "waypoint"; o["x"] = p.x; o["y"] = p.y; o["i"] = i++;
      stat.push(o);
    }
  }
  v["static"] = stat;
  Json ents = Json::array();
  for (size_t i = 0; i < sc.bodies.size(); ++i) {
    Json o = Json::object();
    o["id"] = "body " + std::to_string(i);
    o["k"] = "circle";  // frame layout for this kind: x, y, vx, vy
    o["r"] = sc.bodies[i].r;
    o["m"] = sc.bodies[i].m;
    o["ctrl"] = sc.ctrl.enabled && static_cast<int>(i) == sc.ctrl.body;
    ents.push(o);
  }
  v["ents"] = ents;
  Json frames = Json::array();
  Json energy = Json::array(), force = Json::array();
  for (const StepRecord& rec : tr.rec) {
    Json f = Json::array();
    for (const BodyState& b : rec.body) {
      f.push(r3(b.x)); f.push(r3(b.y)); f.push(r3(b.vx)); f.push(r3(b.vy));
    }
    frames.push(f);
    energy.push(r3(rec.energy));
    force.push(r3(rec.fmag));
  }
  v["frames"] = frames;
  Json sig = Json::object();
  sig["energy"] = energy;
  if (sc.ctrl.enabled) sig["controller force"] = force;
  v["signals"] = sig;
  Json lim = Json::object();
  if (sc.ctrl.enabled) lim["controller force"] = sc.ctrl.fmax;
  v["signal_limits"] = lim;
  Json vj = Json::array();
  for (const Violation& x : viol) vj.push(to_json(x));
  v["violations"] = vj;
  v["scenario"] = to_json(sc);
  v["size"] = scenario_size(sc);
  return v;
}


// ---------------------------------------------------------------- feed compaction

namespace {

std::string brief_for(const std::string& tool, const Json& res) {
  if (!res.bool_or("ok", true)) return "error: " + clip(res.str_or("error", "failed"), 120);
  char b[200];
  if (tool == "run_scenario") {
    if (res.has("rules_violated") && res.at("rules_violated").is_array() && !res.at("rules_violated").arr().empty()) {
      std::string rules;
      for (const auto& r : res.at("rules_violated").arr()) rules += (rules.empty() ? "" : ", ") + r.str();
      int step = res.has("first_violation") ? static_cast<int>(res.at("first_violation").num_or("step", 0)) : 0;
      std::snprintf(b, sizeof b, "%d steps | VIOLATED %s (first at step %d)", static_cast<int>(res.num_or("steps", 0)), rules.c_str(), step);
    } else {
      std::snprintf(b, sizeof b, "%d steps | no rule violated", static_cast<int>(res.num_or("steps", 0)));
    }
    return b;
  }
  if (tool == "shrink") {
    std::snprintf(b, sizeof b, "minimal case found | size %d -> %d | %d candidates tried", static_cast<int>(res.num_or("size_before", 0)),
                  static_cast<int>(res.num_or("size_after", 0)), static_cast<int>(res.num_or("candidates_tried", 0)));
    return b;
  }
  if (tool == "replay") return res.bool_or("identical_trace", false) ? "replayed: identical trace and violations" : "replayed: DIFFERENT from the original";
  if (tool == "inspect_state") return "state at step " + std::to_string(static_cast<int>(res.num_or("step", 0)));
  if (tool == "check_rules") return "rule catalogue returned";
  if (tool == "render_frames") return "frames rendered";
  return "ok";
}

std::string brief_args(const std::string& tool, const Json& a) {
  if (tool == "run_scenario" && a.has("scenario")) {
    const Json& s = a.at("scenario");
    char b[160];
    int nb = s.has("bodies") ? static_cast<int>(s.at("bodies").arr().size()) : 0, nw = s.has("walls") ? static_cast<int>(s.at("walls").arr().size()) : 0;
    std::snprintf(b, sizeof b, "%d %s, %d %s, dt %.4g, %d steps%s", nb, nb == 1 ? "body" : "bodies", nw, nw == 1 ? "wall" : "walls", s.num_or("dt", 0), static_cast<int>(s.num_or("steps", 0)),
                  (s.has("controller") && s.at("controller").bool_or("enabled", false)) ? ", controller on" : "");
    return b;
  }
  if (tool == "shrink") return "rule " + a.str_or("rule", "?");
  if (tool == "inspect_state") return "step " + std::to_string(static_cast<int>(a.num_or("step", 0)));
  if (tool == "render_frames") return "steps " + std::to_string(static_cast<int>(a.num_or("from", a.num_or("start", 0)))) + " to " + std::to_string(static_cast<int>(a.num_or("to", a.num_or("end", 0))));
  if (tool == "submit_report") return "rule " + a.str_or("rule", "?");
  return "";
}

}  // namespace

std::vector<Json> LiveFeed::push(const Json& ev) {
  std::vector<Json> msgs;
  std::string type = ev.str_or("type", "");
  Json out = Json::object();
  out["type"] = type;
  if (type == "assistant_text") {
    out["text"] = ev.str_or("text", "");
  } else if (type == "tool_call") {
    std::string tool = ev.str_or("tool", "");
    out["tool"] = tool;
    if (ev.has("args")) {
      last_args_ = ev.at("args");
      out["args"] = clip(ev.at("args").dump(), 1200);
      out["brief"] = brief_args(tool, ev.at("args"));
      if (tool == "run_scenario" && first_run_.is_null() && ev.at("args").has("scenario")) first_run_ = ev.at("args").at("scenario");
    }
  } else if (type == "tool_result") {
    std::string tool = ev.str_or("tool", "");
    const Json& res = ev.at("result");
    out["tool"] = tool;
    out["ok"] = res.bool_or("ok", true);
    out["summary"] = clip(res.dump(), 600);
    out["brief"] = brief_for(tool, res);
    bool violated = res.has("rules_violated") && res.at("rules_violated").is_array() && !res.at("rules_violated").arr().empty();
    out["violated"] = violated;
    if (tool == "run_scenario" && violated && first_failing_.is_null() && last_args_.is_object() && last_args_.has("scenario"))
      first_failing_ = last_args_.at("scenario");
  } else {
    for (const auto& kv : ev.obj()) {
      if (kv.second.is_string()) out[kv.first] = clip(kv.second.str(), 600);
      else if (kv.second.is_bool()) out[kv.first] = kv.second.boolean();
    }
  }
  Json m = Json::object();
  m["kind"] = "event";
  m["event"] = out;
  msgs.push_back(m);
  // Views for the canvases, as soon as the scenarios exist.
  try {
    if (type == "tool_result" && out.bool_or("violated", false) && !original_sent_ && first_failing_.is_object()) {
      original_sent_ = true;
      Json v = Json::object();
      v["kind"] = "view"; v["which"] = "original"; v["view"] = view_rigid2d(scenario_from_json(first_failing_, domain_), mask_, "original scenario");
      msgs.push_back(v);
    }
    if (type == "tool_result" && ev.str_or("tool", "") == "shrink" && out.bool_or("ok", false) && !shrunk_sent_ && ev.at("result").has("scenario")) {
      shrunk_sent_ = true;
      Json v = Json::object();
      v["kind"] = "view"; v["which"] = "shrunk"; v["view"] = view_rigid2d(scenario_from_json(ev.at("result").at("scenario"), domain_), mask_, "shrunk scenario");
      msgs.push_back(v);
    }
  } catch (const std::exception&) {}
  return msgs;
}

Json build_run_view(const std::string& run_dir, BugMask mask, const std::string& domain) {
  Json doc = Json::object();
  doc["version"] = 1;
  doc["run"] = run_dir.substr(run_dir.find_last_of('/') == std::string::npos ? 0 : run_dir.find_last_of('/') + 1);

  // Event feed (shared with the live stream) and the first scenario the agent ran that violated a rule.
  Json events = Json::array();
  LiveFeed feed(mask);
  std::stringstream es(slurp(run_dir + "/events.jsonl"));
  std::string line;
  while (std::getline(es, line)) {
    if (line.empty()) continue;
    Json ev;
    try { ev = Json::parse(line); } catch (const std::exception&) { continue; }
    for (const Json& m : feed.push(ev))
      if (m.str_or("kind", "") == "event") events.push(m.at("event"));
  }
  doc["events"] = events;
  Json first_failing_args = feed.first_failing(), first_run_args = feed.first_run();

  // Cost totals.
  double usd = 0, latency = 0;
  long long tin = 0, tout = 0;
  int calls = 0;
  std::stringstream cs(slurp(run_dir + "/cost.jsonl"));
  while (std::getline(cs, line)) {
    if (line.empty()) continue;
    try {
      Json c = Json::parse(line);
      usd += c.num_or("cost_usd", 0);
      latency += c.num_or("latency_ms", 0);
      tin += static_cast<long long>(c.num_or("prompt_tokens", 0));
      tout += static_cast<long long>(c.num_or("completion_tokens", 0));
      ++calls;
    } catch (const std::exception&) {}
  }
  Json cost = Json::object();
  cost["usd"] = usd; cost["calls"] = calls; cost["tokens_in"] = static_cast<double>(tin);
  cost["tokens_out"] = static_cast<double>(tout); cost["latency_ms"] = latency;
  doc["cost"] = cost;

  // Report and the two scenarios.
  std::string rep_text = slurp(run_dir + "/report.json");
  Json rep = rep_text.empty() ? Json() : Json::parse(rep_text);
  Json report = Json::object();
  bool valid = false;
  if (rep.is_object()) {
    valid = rep.bool_or("report_valid", false);
    report["valid"] = valid;
    report["stop_reason"] = rep.str_or("stop_reason", "");
    report["rule"] = rep.str_or("rule", "");
    report["explanation"] = rep.str_or("explanation", "");
    report["vision_note"] = rep.str_or("vision_note", "");
    report["vision_agrees"] = rep.bool_or("vision_agrees", false);
    report["vision_done"] = rep.bool_or("vision_done", false);
    report["rejected_reports"] = rep.num_or("rejected_reports", 0);
    if (rep.has("citations")) report["citations"] = rep.at("citations");
  }
  doc["report"] = report;
  Json orig = first_failing_args.is_null() ? first_run_args : first_failing_args;
  try {
    if (orig.is_object()) doc["original"] = view_rigid2d(scenario_from_json(orig, domain), mask, "original scenario");
  } catch (const std::exception&) {}
  if (valid && rep.has("minimal_scenario") && rep.at("minimal_scenario").is_object())
    doc["shrunk"] = view_rigid2d(scenario_from_json(rep.at("minimal_scenario"), domain), mask, "shrunk scenario");
  doc["status"] = valid ? "failure_reported" : (first_failing_args.is_null() ? "no_failure_found" : "failure_not_reported");
  return doc;
}

}  // namespace rc
