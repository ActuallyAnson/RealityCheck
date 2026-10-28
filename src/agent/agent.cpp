#include "agent/agent.hpp"

#include "sim/generic.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <regex>

#include "sim/orbit.hpp"
#include "sim/traffic.hpp"
#include "tools/render.hpp"

namespace rc {
namespace {

Json prop(const char* type, const char* desc) {
  Json p = Json::object();
  p["type"] = type;
  p["description"] = desc;
  return p;
}

Json report_tool_spec(int version = 1) {
  Json cp = Json::object();
  cp["step"] = prop("integer", "A step that exists in the shrunk scenario.");
  cp["body"] = prop("integer", "Body index, or -1 for whole-system values such as energy.");
  cp["field"] = prop("string", "One of x, y, vx, vy, speed, energy.");
  cp["value"] = prop("number", "The value you read from inspect_state at that step (optional but encouraged).");
  cp["claim"] = prop("string", "What this observation shows.");
  Json cit = Json::object();
  cit["type"] = "object";
  cit["properties"] = cp;
  Json creq = Json::array();
  creq.push("step");
  cit["required"] = creq;
  Json citations = Json::object();
  citations["type"] = "array";
  citations["description"] = "Evidence: steps and states you inspected. At least one must be the first violating step.";
  citations["items"] = cit;
  Json props = Json::object();
  props["scenario_id"] = prop("string", "Id of the SHRUNK failing scenario (from shrink).");
  props["rule"] = prop("string", "The violated rule id.");
  props["explanation"] = prop("string", "Likely cause of the failure, grounded in the cited steps.");
  props["citations"] = citations;
  if (version >= 2) {
    Json cf = Json::object();
    cf["scenario_id"] = prop("string", "Id of a scenario you ran that differs from the shrunk scenario by ONE change (at most two fields) and does NOT violate the rule.");
    cf["change"] = prop("string", "What you changed, and why you think it removes the failure.");
    Json cfp = Json::object();
    cfp["type"] = "object";
    cfp["properties"] = cf;
    Json cfr = Json::array();
    cfr.push("scenario_id");
    cfp["required"] = cfr;
    cfp["description"] = "Your causal test: the single change that makes the failure disappear.";
    props["counterfactual"] = cfp;
  }
  Json params = Json::object();
  params["type"] = "object";
  params["properties"] = props;
  Json req = Json::array();
  for (const char* r : {"scenario_id", "rule", "explanation", "citations"}) req.push(r);
  if (version >= 2) req.push("counterfactual");
  params["required"] = req;
  Json f = Json::object();
  f["name"] = "submit_report";
  f["description"] =
      "Submit the final failure report once you have a shrunk, replayed failure. The report is rejected if it cites "
      "steps, bodies or values that do not exist in the recorded run; fix it and resubmit.";
  f["parameters"] = params;
  return f;
}

Json all_tools(const std::string& domain = "rigid2d", int version = 1) {
  Json t = tool_specs(domain);
  t.push(report_tool_spec(version));
  return t;
}

std::string truncate(const std::string& s, size_t n) {
  if (s.size() <= n) return s;
  return s.substr(0, n) + "...[truncated " + std::to_string(s.size() - n) + " chars]";
}

// Extracts the first balanced JSON object from model text (handles code fences and prose around it).
bool extract_json_object(const std::string& text, Json* out) {
  size_t start = text.find('{');
  while (start != std::string::npos) {
    int depth = 0;
    bool in_str = false;
    for (size_t i = start; i < text.size(); ++i) {
      char c = text[i];
      if (in_str) {
        if (c == '\\') ++i;
        else if (c == '"') in_str = false;
      } else if (c == '"') {
        in_str = true;
      } else if (c == '{') {
        ++depth;
      } else if (c == '}' && --depth == 0) {
        try { *out = Json::parse(text.substr(start, i - start + 1)); return true; } catch (const std::exception&) {}
        break;
      }
    }
    start = text.find('{', start + 1);
  }
  return false;
}

double field_value(const Scenario& sc, const StepRecord& rec, int body, const std::string& f, bool* ok) {
  *ok = true;
  if (sc.domain == "generic") {  // a citation names one of the model card's signals
    const GenericModel& gm = generic_model();
    for (size_t i = 0; i < gm.signals.size() && i < rec.body.size(); ++i) if (gm.signals[i].name == f) return rec.body[i].x;
    *ok = false;
    return 0;
  }
  if (f == "energy") return rec.energy;
  if (sc.domain == "traffic") {  // per-approach values: body = approach index
    if (body < 0 || body >= static_cast<int>(rec.body.size())) { *ok = false; return 0; }
    const BodyState& q = rec.body[static_cast<size_t>(body)];
    if (f == "signal") return q.x;
    if (f == "queue") return q.y;
    if (f == "wait") return q.vx;
    if (f == "flag") return q.vy;
  }
  if (sc.domain == "cartpole" && !rec.body.empty()) {  // body index is irrelevant: one cart-pole
    const BodyState& q = rec.body[0];
    if (f == "x") return q.x;
    if (f == "v") return q.vx;
    if (f == "theta") return q.y;
    if (f == "omega") return q.vy;
    if (f == "controller_force") return rec.fmag;
    if (f == "external_work") return rec.work;
  }
  if (sc.domain == "orbit") {  // whole-system quantities reported by inspect_state
    double ke, pe;
    if (f == "angular_momentum") return rec.ang;
    if (f == "momentum_x") return rec.px;
    if (f == "momentum_y") return rec.py;
    if (f == "kinetic" || f == "potential") { orbit_energies(sc, rec, &ke, &pe); return f == "kinetic" ? ke : pe; }
  }
  if (body < 0 || body >= static_cast<int>(rec.body.size())) { *ok = false; return 0; }
  const BodyState& q = rec.body[static_cast<size_t>(body)];
  if (f == "x") return q.x;
  if (f == "y") return q.y;
  if (f == "vx") return q.vx;
  if (f == "vy") return q.vy;
  if (f == "speed") return std::sqrt(q.vx * q.vx + q.vy * q.vy);
  *ok = false;
  return 0;
}

}  // namespace

CitationCheck validate_report(const ToolSession::Entry& run, bool shrunk, const Json& report) {
  CitationCheck c;
  auto bad = [&](const std::string& m) { c.problems.push_back(m); };
  const int steps = run.tr.steps();
  const int nbodies = run.sc.domain == "cartpole" ? 1 : run.sc.domain == "traffic" ? 4 : static_cast<int>(run.sc.bodies.size());
  if (!shrunk) bad("scenario_id must be the shrunk scenario returned by the shrink tool");
  std::string rule = report.str_or("rule", "");
  const Violation* viol = nullptr;
  for (const auto& v : run.viol) if (v.rule == rule) { viol = &v; break; }
  if (!viol) bad("rule '" + rule + "' is not violated in this scenario");
  std::string text = report.str_or("explanation", "");
  if (text.size() < 20) bad("explanation is empty or too short");

  // Free-text mentions of steps and bodies must exist.
  static const std::regex step_re("steps?\\s*#?(\\d+)", std::regex::icase);
  static const std::regex body_re("bod(?:y|ies)\\s*(?:index\\s*)?#?(\\d+)", std::regex::icase);
  for (std::sregex_iterator it(text.begin(), text.end(), step_re), end; it != end; ++it) {
    long n = std::stol((*it)[1].str());
    if (n > steps)
      bad("explanation mentions step " + std::to_string(n) + " but the scenario has steps 0.." + std::to_string(steps));
  }
  for (std::sregex_iterator it(text.begin(), text.end(), body_re), end; it != end; ++it) {
    long n = std::stol((*it)[1].str());
    if (n >= nbodies)
      bad("explanation mentions body " + std::to_string(n) + " but the scenario has bodies 0.." + std::to_string(nbodies - 1));
  }

  bool cites_violation_step = false;
  if (!report.has("citations") || !report.at("citations").is_array() || report.at("citations").arr().empty()) {
    bad("at least one citation is required");
  } else {
    int idx = 0;
    for (const Json& cj : report.at("citations").arr()) {
      ++idx;
      std::string tag = "citation " + std::to_string(idx) + ": ";
      if (!cj.is_object() || !cj.has("step") || !cj.at("step").is_number()) { bad(tag + "missing integer step"); continue; }
      int step = static_cast<int>(cj.at("step").num());
      if (step < 0 || step > steps) {
        bad(tag + "step " + std::to_string(step) + " does not exist (valid 0.." + std::to_string(steps) + ")");
        continue;
      }
      if (viol && step == viol->step) cites_violation_step = true;
      int body = static_cast<int>(cj.num_or("body", -1));
      if (body < -1 || body >= nbodies) { bad(tag + "body " + std::to_string(body) + " does not exist"); continue; }
      if (cj.has("value") && cj.at("value").is_number()) {
        std::string f = cj.str_or("field", "");
        bool ok;
        double actual = field_value(run.sc, run.tr.rec[static_cast<size_t>(step)], body, f, &ok);
        if (!ok) { bad(tag + "cannot check value: unknown field '" + f + "' for body " + std::to_string(body)); continue; }
        double claimed = cj.at("value").num();
        double tol = std::max(0.02, 0.01 * std::fabs(actual));
        if (!(std::fabs(claimed - actual) <= tol)) {
          char b[240];
          std::snprintf(b, sizeof b, "%sclaimed %s=%.4g at step %d but the recorded value is %.4g", tag.c_str(),
                        f.c_str(), claimed, step, actual);
          bad(b);
        }
      }
    }
  }
  if (viol && !cites_violation_step)
    bad("no citation refers to the first violating step (" + std::to_string(viol->step) + ")");
  c.ok = c.problems.empty();
  return c;
}

namespace {
void collect_numbers(const Json& j, std::vector<double>* out) {
  if (j.is_number()) out->push_back(j.num());
  else if (j.is_array()) for (const Json& e : j.arr()) collect_numbers(e, out);
  else if (j.is_object()) for (const auto& kv : j.obj()) collect_numbers(kv.second, out);
}

void collect_record(const StepRecord& r, std::vector<double>* out) {
  for (const BodyState& b : r.body) {
    out->push_back(b.x); out->push_back(b.y); out->push_back(b.vx); out->push_back(b.vy);
    out->push_back(std::sqrt(b.vx * b.vx + b.vy * b.vy));
  }
  out->push_back(r.energy); out->push_back(r.work); out->push_back(r.fmag); out->push_back(r.px); out->push_back(r.py); out->push_back(r.ang);
}

// Does the token "text" (as written) match a number: by rounding to the digits written, or within a relative error.
bool near_token(const std::string& text, double t, double v, double rel) {
  size_t dot = text.find('.');
  int decimals = dot == std::string::npos ? 0 : static_cast<int>(text.size() - dot - 1);
  double half = 0.5 * std::pow(10.0, -decimals) * 1.0001;
  double a = std::fabs(v), b = std::fabs(t);
  return std::fabs(a - b) <= half || std::fabs(a - b) <= rel * std::max(a, b);
}

bool prose_matches(const std::string& text, double t, const std::vector<double>& pool, double rel, bool derived) {
  for (double v : pool) if (near_token(text, t, v, rel)) return true;
  if (!derived) return false;
  // derived quantities: combine two distinct recorded values (pool deduplicated and capped so this stays cheap and cannot match everything)
  std::vector<double> u;
  for (double v : pool) if (std::isfinite(v) && std::fabs(v) < 1e6) u.push_back(v);
  std::sort(u.begin(), u.end());
  u.erase(std::unique(u.begin(), u.end(), [](double x, double y) { return std::fabs(x - y) < 1e-9; }), u.end());
  if (u.size() > 600) return false;
  for (double a : u) {
    if (near_token(text, t, a / 2, rel) || near_token(text, t, a * 100, rel)) return true;
    for (double b : u) {
      if (near_token(text, t, a - b, rel) || near_token(text, t, a + b, rel) || near_token(text, t, a * b, rel) || (b != 0 && near_token(text, t, a / b, rel)) ||
          (b != 0 && near_token(text, t, 100 * (a - b) / b, rel)))
        return true;
    }
  }
  return false;
}

// Names a scenario path can be called by: each dotted segment without its index ("bodies[0].vx" -> bodies, vx).
std::vector<std::string> path_names(const std::string& path) {
  std::vector<std::string> out;
  std::string cur;
  auto flush = [&]() { if (cur.size() >= 2) out.push_back(cur); cur.clear(); };
  for (char ch : path) {
    if (ch == '.') flush();
    else if (ch == '[') { flush(); }
    else if (ch == ']') cur.clear();
    else cur += ch;
  }
  flush();
  return out;
}
}  // namespace

CitationCheck validate_report_strict(const ToolSession::Entry& run, const ToolSession::Entry* cf, bool shrunk, const Json& report,
                                     const StrictOptions& opt) {
  CitationCheck c = validate_report(run, shrunk, report);
  auto bad = [&](const std::string& m) { c.problems.push_back(m); };
  const int steps = run.tr.steps();
  std::string rule = report.str_or("rule", "");
  const Violation* viol = nullptr;
  for (const auto& v : run.viol) if (v.rule == rule) { viol = &v; break; }
  std::vector<double> pool;
  collect_record(run.tr.rec.front(), &pool);
  collect_record(run.tr.rec.back(), &pool);
  collect_numbers(to_json(run.sc), &pool);
  if (viol) { pool.push_back(viol->value); pool.push_back(viol->limit); collect_record(run.tr.rec[static_cast<size_t>(viol->step)], &pool); }
  if (cf) collect_numbers(to_json(cf->sc), &pool);
  if (opt.seen) pool.insert(pool.end(), opt.seen->begin(), opt.seen->end());
  if (report.has("citations") && report.at("citations").is_array()) {
    int idx = 0;
    for (const Json& cj : report.at("citations").arr()) {
      ++idx;
      std::string tag = "strict citation " + std::to_string(idx) + ": ";
      if (!cj.is_object() || !cj.has("step") || !cj.at("step").is_number()) continue;
      int step = static_cast<int>(cj.at("step").num());
      if (step < 0 || step > steps) continue;
      collect_record(run.tr.rec[static_cast<size_t>(step)], &pool);
      bool ok = false;
      double actual = cj.str_or("field", "").empty() ? 0 : field_value(run.sc, run.tr.rec[static_cast<size_t>(step)], static_cast<int>(cj.num_or("body", -1)), cj.str_or("field", ""), &ok);
      if (ok && !std::isfinite(actual)) continue;  // a NaN or infinity cannot be written as a JSON value
      if (!cj.has("value") || !cj.at("value").is_number() || cj.str_or("field", "").empty()) { bad(tag + "needs a field and a numeric value"); continue; }
      if (!ok) continue;  // already reported by the default check
      double tol = std::max(opt.abs_tol, opt.rel_tol * std::fabs(actual));
      if (!(std::fabs(cj.at("value").num() - actual) <= tol)) bad(tag + "value is outside the strict tolerance");
    }
  }
  if (opt.prose_numbers) {
    std::string text = report.str_or("explanation", "");
    if (report.has("citations") && report.at("citations").is_array())
      for (const Json& cj : report.at("citations").arr()) if (cj.is_object()) text += " " + cj.str_or("claim", "");
    static const std::regex num_re("(?:^|[^A-Za-z0-9_.])([0-9]+\\.[0-9]+(?:[eE][-+]?[0-9]+)?|[0-9]{4,})(?![0-9A-Za-z_])");
    for (std::sregex_iterator it(text.begin(), text.end(), num_re), end; it != end; ++it) {
      std::string tok = (*it)[1].str();
      double t = std::strtod(tok.c_str(), nullptr);
      if (tok.find('.') == std::string::npos && t <= steps + 1) continue;  // a step count or index
      if (!prose_matches(tok, t, pool, opt.prose_rel, opt.derived)) bad("strict prose: number " + tok + " matches no recorded value");
    }
  }
  if (opt.tie_cause && cf) {
    std::vector<std::string> paths = scenario_diff(to_json(run.sc), to_json(cf->sc));
    std::string hay = report.str_or("explanation", "");
    if (report.has("counterfactual") && report.at("counterfactual").is_object()) hay += " " + report.at("counterfactual").str_or("change", "");
    bool named = paths.empty();
    for (const std::string& p : paths) {
      for (const std::string& nm : path_names(p)) {
        std::regex w("\\b" + nm + "\\b", std::regex::icase);
        if (std::regex_search(hay, w)) named = true;
      }
    }
    if (!named) bad("strict cause: neither the explanation nor the counterfactual note names a field the counterfactual changed");
  }
  c.ok = c.problems.empty();
  return c;
}

std::string system_prompt(bool native_tools, const std::string& domain, int version) {
  std::string p =
      "You are RealityCheck, an adversarial tester for a physics simulation. Your job is to find scenarios where the "
      "simulation breaks its declared rules, confirm them, and explain the likely cause.\n\n" +
      simulation_description(domain) +
      "\nHOW YOU WORK\n"
      "- You do not decide pass or fail. The deterministic engine does, through the tools. You decide where to look.\n"
      "- Form a specific hypothesis about how the simulation could break a rule, design the smallest scenario that tests it, "
      "run it, read the result, and refine. State each hypothesis in one or two sentences before you act.\n"
      "- Try extremes and degenerate values of every parameter, interactions between features, and different numerical regimes. "
      "Do not repeat a scenario that already passed; vary what you learned mattered.\n"
      "- Your hunt budget (simulated steps and tool calls) is limited; every run_scenario call spends it. Short, targeted "
      "scenarios are cheaper than long random ones.\n"
      "- When run_scenario reports a violation: call shrink, then replay on the shrunk scenario, then inspect_state at the "
      "first violating step and a few steps before it (render_frames is optional), then call submit_report.\n"
      "- submit_report is rejected if it cites steps, bodies or values that do not exist. Cite only what you inspected, "
      "and include a citation for the first violating step. Fix and resubmit if rejected.\n"
      "- Use only the information the tools return. You cannot see the simulation's source code.\n";
  if (native_tools) {
    p += "- Call exactly one tool per turn.\n";
  } else {
    p += "\nACTION PROTOCOL\nReply with exactly one JSON object and nothing else: {\"tool\": \"<name>\", \"args\": {...}}. "
         "The next message will contain the tool result. Available tools:\n" +
         all_tools(domain, version).dump() + "\n";
  }
  if (version >= 2) {
    p +=
        "\nVERSION 2 PROCEDURE (in addition to the above)\n"
        "- Keep a short running ledger in your text before each action: the hypotheses still open, the ones ruled out and why.\n"
        "- After shrink, do not stop at 'it fails'. Test your explanation by intervention: run_scenario on the shrunk scenario with ONE "
        "change (at most two fields) that you predict removes the failure. That scenario must run the same number of steps. If it still "
        "fails, your hypothesis is wrong: revise it.\n"
        "- submit_report must include `counterfactual` with the id of that passing scenario. It is rejected if the scenario does not "
        "exist, still violates the rule, differs from the shrunk scenario in more than two fields, or is too short to show the failure.\n"
        "- render_frames output may be shown to you as an image: use it to check that your picture of what happens matches the numbers.\n";
  }
  return p;
}

namespace {

struct Loop {
  BudgetedModel& model;
  ToolSession& tools;
  const AgentConfig& cfg;
  const EventSink& sink;
  AgentResult res;
  std::vector<ChatMessage> msgs;

  void emit(const char* type, Json data = Json::object()) {
    if (!sink) return;
    data["type"] = type;
    sink(data);
  }

  ModelRequest make_request() const {
    ModelRequest r;
    r.model = cfg.model;
    r.messages = msgs;
    if (cfg.native_tools) r.tools = all_tools(cfg.domain, cfg.version);
    r.max_tokens = cfg.max_tokens;
    r.reasoning_effort = cfg.reasoning_effort;
    return r;
  }

  void add_tool_result(const ToolCall& call, const Json& result) {
    std::string body = truncate(result.dump(), cfg.tool_result_chars);
    ChatMessage m;
    if (cfg.native_tools) {
      m.role = "tool";
      m.tool_call_id = call.id;
      m.content = body;
    } else {
      m.role = "user";
      m.content = "TOOL_RESULT " + call.name + ": " + body;
    }
    msgs.push_back(m);
  }

  // v2: the counterfactual must be a passing, one-change variant of the shrunk scenario, run for long enough.
  bool check_counterfactual(const ToolSession::Entry& run, const std::string& id, const Json& args, Json* out_cf,
                            std::vector<std::string>* problems) {
    if (!args.has("counterfactual") || !args.at("counterfactual").is_object() || !args.at("counterfactual").has("scenario_id")) {
      problems->push_back("counterfactual.scenario_id is required");
      return false;
    }
    const Json& cf = args.at("counterfactual");
    std::string cid = cf.str_or("scenario_id", "");
    const ToolSession::Entry* c = tools.record(cid);
    if (!c) { problems->push_back("counterfactual: unknown scenario_id " + cid); return false; }
    if (cid == id) { problems->push_back("counterfactual must differ from the shrunk scenario"); return false; }
    std::string rule = args.str_or("rule", "");
    for (const auto& v : c->viol)
      if (v.rule == rule) { problems->push_back("counterfactual still violates " + rule + " (at step " + std::to_string(v.step) + "): your hypothesis does not remove the failure"); return false; }
    int vstep = 0;
    for (const auto& v : run.viol) if (v.rule == rule) { vstep = v.step; break; }
    if (c->sc.steps < vstep) { problems->push_back("counterfactual runs only " + std::to_string(c->sc.steps) + " steps, before the failure step " + std::to_string(vstep)); return false; }
    std::vector<std::string> paths = scenario_diff(to_json(run.sc), to_json(c->sc));
    if (paths.empty()) { problems->push_back("counterfactual is identical to the shrunk scenario"); return false; }
    if (paths.size() > 2) { problems->push_back("counterfactual changes " + std::to_string(paths.size()) + " fields; change at most two"); return false; }
    Json o = Json::object();
    o["scenario_id"] = cid;
    o["change"] = cf.str_or("change", "");
    Json pj = Json::array();
    for (const auto& p : paths) pj.push(p);
    o["changed_paths"] = pj;
    *out_cf = o;
    return true;
  }

  // Handles submit_report. Returns true when the report was accepted.
  bool handle_report(const ToolCall& call, const Json& args) {
    ++res.report_attempts;
    res.report_submitted = true;
    std::string id = args.str_or("scenario_id", "");
    const ToolSession::Entry* run = tools.record(id);
    Json result = Json::object();
    if (!run) {
      result["ok"] = false;
      result["error"] = "unknown scenario_id";
      ++res.rejected_reports;
      emit("report_rejected", result);
      add_tool_result(call, result);
      return false;
    }
    CitationCheck chk = validate_report(*run, tools.is_shrunk(id), args);
    Json cf_ok;
    if (cfg.version >= 2) {
      std::vector<std::string> cfp;
      if (!check_counterfactual(*run, id, args, &cf_ok, &cfp)) {
        chk.ok = false;
        for (auto& p : cfp) chk.problems.push_back(p);
      }
    }
    if (!chk.ok) {
      Json probs = Json::array();
      for (const auto& s : chk.problems) probs.push(s);
      result["ok"] = false;
      result["error"] = "report rejected: it does not match the recorded run";
      result["problems"] = probs;
      ++res.rejected_reports;
      emit("report_rejected", result);
      add_tool_result(call, result);
      return false;
    }
    res.report_valid = true;
    res.scenario_id = id;
    res.rule = args.str_or("rule", "");
    res.explanation = args.str_or("explanation", "");
    res.minimal_scenario = to_json(run->sc);
    if (cfg.version >= 2) res.counterfactual = cf_ok;
    for (const auto& v : run->viol) if (v.rule == res.rule) { res.violation = to_json(v); break; }
    for (const Json& cj : args.at("citations").arr()) {
      Citation c;
      c.step = static_cast<int>(cj.num_or("step", 0));
      c.body = static_cast<int>(cj.num_or("body", -1));
      c.field = cj.str_or("field", "");
      c.has_value = cj.has("value") && cj.at("value").is_number();
      c.value = c.has_value ? cj.at("value").num() : 0;
      c.claim = cj.str_or("claim", "");
      res.citations.push_back(c);
    }
    result["ok"] = true;
    result["message"] = "report accepted";
    emit("report_accepted", result);
    add_tool_result(call, result);
    return true;
  }

  // Vision cross-check: show the model the frames around the first violation and ask whether they agree.
  void vision_step() {
    if (!res.report_valid) return;
    int vstep = static_cast<int>(res.violation.num_or("step", 0));
    const ToolSession::Entry* run = tools.record(res.scenario_id);
    if (!run) return;
    int from = std::max(0, vstep - 3), to = std::min(run->tr.steps(), vstep + 3);
    Json a = Json::object();
    a["scenario_id"] = res.scenario_id;
    a["from"] = from;
    a["to"] = to;
    a["highlight_body"] = static_cast<int>(res.violation.num_or("body", -1));
    Json r = tools.call("render_frames", a);
    if (!r.at("ok").boolean()) return;
    res.frame_strip_path = r.at("path").str();
    std::ifstream f(res.frame_strip_path, std::ios::binary);
    std::vector<uint8_t> png((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    ChatMessage m;
    m.role = "user";
    m.images_b64.push_back(base64_encode(png));
    m.content =
        "Frame strip of the shrunk failing scenario around the first violation (step " + std::to_string(vstep) +
        "). Tiles are labelled with their step number; orange = controller-driven body, red = the violating body, "
        "grey = walls, green crosses = waypoints. Violated rule: " + res.rule + ".\nYour numeric explanation was:\n" +
        res.explanation + "\n\nDescribe in two sentences what visibly goes wrong, then say whether the picture agrees with the "
        "explanation. Reply with a JSON object only: {\"description\": string, \"agrees\": boolean, \"disagreement\": string}.";
    ModelRequest req;
    req.model = cfg.model;
    req.messages = {msgs.front(), m};  // system prompt + the image question; no tools
    req.max_tokens = cfg.max_tokens;
    req.reasoning_effort = cfg.reasoning_effort;
    try {
      ModelResponse resp = model.complete(req, "vision");
      if (!resp.ok) { res.vision_note = "vision call failed: " + resp.error; return; }
      Json j;
      if (extract_json_object(resp.message.content, &j)) {
        res.vision_note = j.str_or("description", resp.message.content);
        res.vision_agrees = j.bool_or("agrees", false);
        std::string dis = j.str_or("disagreement", "");
        if (!dis.empty() && !res.vision_agrees) res.vision_note += " [disagreement: " + dis + "]";
      } else {
        res.vision_note = resp.message.content;
      }
      res.vision_done = true;
      Json ev = Json::object();
      ev["description"] = res.vision_note;
      ev["agrees"] = res.vision_agrees;
      emit("vision", ev);
    } catch (const BudgetExceeded& e) {
      res.vision_note = std::string("skipped: ") + e.what();
    }
  }

  AgentResult run() {
    ChatMessage sys;
    sys.role = "system";
    sys.content = system_prompt(cfg.native_tools, cfg.domain, cfg.version);
    msgs.push_back(sys);
    ChatMessage task;
    task.role = "user";
    task.content =
        "Begin. Hunt for a rule violation in the simulation, shrink it, and submit a grounded report. Hunt budget: " +
        std::to_string(tools.budget().max_steps) + " simulated steps, " + std::to_string(tools.budget().max_tool_calls) +
        " tool calls.";
    if (cfg.triage.is_object()) {
      Json a = Json::object();
      a["scenario"] = cfg.triage;
      Json r = tools.call("run_scenario", a);
      emit("triage_seed", r);
      if (r.bool_or("ok", false) && r.has("first_violation")) {
        const Json& v = r.at("first_violation");
        task.content =
            "A fuzzer has already found a failing scenario; do NOT hunt for new ones. It is stored as scenario_id " + r.str_or("scenario_id", "") +
            " and violates rule " + v.str_or("rule", "") + " first at step " + std::to_string(static_cast<int>(v.num_or("step", 0))) + " (" +
            v.str_or("detail", "") + "). Your job is triage: call shrink, then replay, inspect_state at the first violating step and a few steps before, "
            "work out the cause, test it with one counterfactual change, and submit_report. Tool budget: " + std::to_string(tools.budget().max_tool_calls) + " calls.";
      }
    }
    msgs.push_back(task);

    int idle = 0, model_errors = 0;
    for (res.turns = 1; res.turns <= cfg.max_turns; ++res.turns) {
      ModelResponse resp;
      try {
        resp = model.complete(make_request(), "hunt");
      } catch (const BudgetExceeded& e) {
        res.stop_reason = "model_budget";
        Json ev = Json::object();
        ev["message"] = e.what();
        emit("stopped", ev);
        break;
      }
      if (!resp.ok) {
        Json ev = Json::object();
        ev["error"] = resp.error;
        emit("model_error", ev);
        if (++model_errors >= 3) { res.stop_reason = "model_error"; break; }
        continue;
      }
      std::vector<ToolCall> calls = resp.message.tool_calls;
      if (!cfg.native_tools) {
        calls.clear();
        Json j;
        if (extract_json_object(resp.message.content, &j) && j.has("tool") && j.at("tool").is_string()) {
          ToolCall tc;
          tc.id = "call_" + std::to_string(res.turns);
          tc.name = j.at("tool").str();
          tc.arguments = (j.has("args") ? j.at("args") : Json::object()).dump();
          calls.push_back(tc);
        }
      }
      if (calls.size() > 1) calls.resize(1);  // one action per turn keeps tool-call ids matched
      ChatMessage am = resp.message;
      am.tool_calls = cfg.native_tools ? calls : std::vector<ToolCall>();
      msgs.push_back(am);
      if (!resp.message.content.empty()) {
        Json ev = Json::object();
        ev["text"] = resp.message.content;
        emit("assistant_text", ev);
      }
      if (calls.empty()) {
        if (++idle >= 3) { res.stop_reason = "no_tool_calls"; break; }
        ChatMessage nudge;
        nudge.role = "user";
        nudge.content = cfg.native_tools ? "Call one of the tools." : "Reply with exactly one JSON action object.";
        msgs.push_back(nudge);
        continue;
      }
      idle = 0;
      const ToolCall call = calls[0];
      Json args;
      try {
        args = call.arguments.empty() ? Json::object() : Json::parse(call.arguments);
      } catch (const std::exception&) {
        Json bad = Json::object();
        bad["ok"] = false;
        bad["error"] = "arguments are not valid JSON";
        add_tool_result(call, bad);
        continue;
      }
      Json ev = Json::object();
      ev["tool"] = call.name;
      ev["args"] = args;
      emit("tool_call", ev);
      if (call.name == "submit_report") {
        if (handle_report(call, args)) { res.stop_reason = "report_accepted"; break; }
        if (res.report_attempts >= cfg.max_report_attempts) { res.stop_reason = "report_rejected"; break; }
        continue;
      }
      Json result = tools.call(call.name, args);
      Json rev = Json::object();
      rev["tool"] = call.name;
      rev["result"] = result;
      emit("tool_result", rev);
      add_tool_result(call, result);
      if (cfg.version >= 2 && cfg.vision_in_loop && call.name == "render_frames" && result.bool_or("ok", false) && result.has("path")) {
        std::ifstream f(result.at("path").str(), std::ios::binary);
        std::vector<uint8_t> png((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (!png.empty()) {
          ChatMessage im;
          im.role = "user";
          im.images_b64.push_back(base64_encode(png));
          im.content = "The frames you just rendered (blue = bodies; read them against the numbers).";
          msgs.push_back(im);
          Json iev = Json::object();
          iev["note"] = "frames attached to the conversation";
          emit("frames_shown", iev);
        }
      }
      if (tools.budget_exhausted() && !tools.has_failure()) { res.stop_reason = "tool_budget"; break; }
    }
    if (res.stop_reason.empty()) res.stop_reason = "turns";
    res.found_failure = tools.has_failure();
    vision_step();
    return res;
  }
};

}  // namespace

namespace {
void diff_rec(const Json& a, const Json& b, const std::string& path, std::vector<std::string>* out) {
  if (a.is_object() && b.is_object()) {
    for (const auto& kv : a.obj()) {
      std::string p = path.empty() ? kv.first : path + "." + kv.first;
      if (!b.has(kv.first)) out->push_back(p);
      else diff_rec(kv.second, b.at(kv.first), p, out);
    }
    for (const auto& kv : b.obj()) if (!a.has(kv.first)) out->push_back(path.empty() ? kv.first : path + "." + kv.first);
  } else if (a.is_array() && b.is_array()) {
    size_t n = std::min(a.arr().size(), b.arr().size());
    if (a.arr().size() != b.arr().size()) out->push_back(path + ".length");
    for (size_t i = 0; i < n; ++i) diff_rec(a.arr()[i], b.arr()[i], path + "[" + std::to_string(i) + "]", out);
  } else if (a.dump() != b.dump()) {
    out->push_back(path);
  }
}
}  // namespace

std::vector<std::string> scenario_diff(const Json& a, const Json& b) {
  std::vector<std::string> out;
  diff_rec(a, b, "", &out);
  return out;
}

AgentResult run_agent(BudgetedModel& model, ToolSession& tools, const AgentConfig& cfg, const EventSink& sink) {
  Loop l{model, tools, cfg, sink, AgentResult(), {}};
  return l.run();
}

Json agent_result_to_json(const AgentResult& r) {
  Json j = Json::object();
  j["found_failure"] = r.found_failure;
  j["report_submitted"] = r.report_submitted;
  j["report_valid"] = r.report_valid;
  j["report_attempts"] = r.report_attempts;
  j["rejected_reports"] = r.rejected_reports;
  j["stop_reason"] = r.stop_reason;
  j["turns"] = r.turns;
  j["scenario_id"] = r.scenario_id;
  j["rule"] = r.rule;
  j["minimal_scenario"] = r.minimal_scenario;
  j["violation"] = r.violation;
  j["explanation"] = r.explanation;
  Json cits = Json::array();
  for (const auto& c : r.citations) {
    Json o = Json::object();
    o["step"] = c.step;
    o["body"] = c.body;
    o["field"] = c.field;
    o["claim"] = c.claim;
    if (c.has_value) o["value"] = c.value;
    cits.push(o);
  }
  j["citations"] = cits;
  j["frame_strip_path"] = r.frame_strip_path;
  j["vision_note"] = r.vision_note;
  j["vision_agrees"] = r.vision_agrees;
  j["vision_done"] = r.vision_done;
  if (r.counterfactual.is_object()) j["counterfactual"] = r.counterfactual;
  return j;
}

void write_report_files(const AgentResult& r, const ToolSession& tools, const std::string& dir) {
  std::ofstream(dir + "/report.json") << agent_result_to_json(r).dump() << "\n";
  const ToolSession::Entry* run = tools.record(r.scenario_id);
  if (!run) return;
  Json t = Json::object();
  t["scenario"] = to_json(run->sc);
  Json frames = Json::array();
  for (const auto& rec : run->tr.rec) {
    Json f = Json::object();
    Json bodies = Json::array();
    for (const auto& q : rec.body) {
      Json b = Json::array();
      b.push(q.x);
      b.push(q.y);
      bodies.push(b);
    }
    f["b"] = bodies;
    f["e"] = rec.energy;
    frames.push(f);
  }
  t["frames"] = frames;
  Json v = Json::array();
  for (const auto& x : run->viol) v.push(to_json(x));
  t["violations"] = v;
  std::ofstream(dir + "/trace.json") << t.dump() << "\n";
}

}  // namespace rc
