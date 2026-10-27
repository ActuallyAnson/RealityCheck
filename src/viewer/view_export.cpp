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

Json view_rigid2d(const Scenario& sc, BugMask mask, const std::string& label) {
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
  return v;
}

Json build_run_view(const std::string& run_dir, BugMask mask) {
  Json doc = Json::object();
  doc["version"] = 1;
  doc["run"] = run_dir.substr(run_dir.find_last_of('/') == std::string::npos ? 0 : run_dir.find_last_of('/') + 1);

  // Event feed, and the first scenario the agent ran that violated a rule.
  Json events = Json::array();
  Json first_failing_args, first_run_args;
  Json last_args;
  std::stringstream es(slurp(run_dir + "/events.jsonl"));
  std::string line;
  while (std::getline(es, line)) {
    if (line.empty()) continue;
    Json ev;
    try { ev = Json::parse(line); } catch (const std::exception&) { continue; }
    std::string type = ev.str_or("type", "");
    Json out = Json::object();
    out["type"] = type;
    if (type == "assistant_text") {
      out["text"] = ev.str_or("text", "");
    } else if (type == "tool_call") {
      out["tool"] = ev.str_or("tool", "");
      if (ev.has("args")) {
        last_args = ev.at("args");
        out["args"] = clip(ev.at("args").dump(), 260);
        if (ev.str_or("tool", "") == "run_scenario" && first_run_args.is_null() && ev.at("args").has("scenario"))
          first_run_args = ev.at("args").at("scenario");
      }
    } else if (type == "tool_result") {
      out["tool"] = ev.str_or("tool", "");
      const Json& res = ev.at("result");
      out["ok"] = res.bool_or("ok", true);
      out["summary"] = clip(res.dump(), 240);
      out["violated"] = res.has("rules_violated") && res.at("rules_violated").is_array() && !res.at("rules_violated").arr().empty();
      if (ev.str_or("tool", "") == "run_scenario" && res.has("rules_violated") && res.at("rules_violated").is_array() &&
          !res.at("rules_violated").arr().empty() && first_failing_args.is_null() && last_args.is_object() && last_args.has("scenario"))
        first_failing_args = last_args.at("scenario");
    } else {
      for (const auto& kv : ev.obj()) {
        if (kv.second.is_string()) out[kv.first] = clip(kv.second.str(), 400);
        else if (kv.second.is_bool()) out[kv.first] = kv.second.boolean();
      }
    }
    events.push(out);
  }
  doc["events"] = events;

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
    if (orig.is_object()) doc["original"] = view_rigid2d(scenario_from_json(orig), mask, "original scenario");
  } catch (const std::exception&) {}
  if (valid && rep.has("minimal_scenario") && rep.at("minimal_scenario").is_object())
    doc["shrunk"] = view_rigid2d(scenario_from_json(rep.at("minimal_scenario")), mask, "shrunk scenario");
  doc["status"] = valid ? "failure_reported" : (first_failing_args.is_null() ? "no_failure_found" : "failure_not_reported");
  return doc;
}

}  // namespace rc
