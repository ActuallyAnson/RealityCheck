#include "api/api.hpp"

#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "baseline/random_baseline.hpp"
#include "external/bundle.hpp"
#include "external/engine.hpp"
#include "rules/checker.hpp"
#include "shrink/shrink.hpp"
#include "sim/domains.hpp"
#include "spec/spec.hpp"
#include "tools/tools.hpp"

namespace rc {
namespace {

Json err(const std::string& m) {
  Json j = Json::object();
  j["ok"] = false;
  j["error"] = m;
  return j;
}

bool known_domain(const std::string& d) { return d == "rigid2d" || d == "orbit" || d == "cartpole" || d == "traffic" || d == "generic"; }

struct Ctx {
  std::string domain;
  SimFn sim;
};

// Parses domain + engine; the engine spec is validated before use. Returns an error object (non-null) on failure.
bool make_ctx(const Json& a, const ApiOptions& opt, Ctx* c, Json* e) {
  c->domain = a.str_or("domain", "rigid2d");
  if (!known_domain(c->domain)) { *e = err("unknown domain (rigid2d, orbit, cartpole, traffic)"); return false; }
  std::string spec = a.str_or("engine", "builtin");
  if (spec.compare(0, 5, "exec:") == 0 && !opt.allow_exec) { *e = err("exec: engines are disabled on this server (start it with --allow-exec to enable them locally)"); return false; }
  try { c->sim = make_engine(spec, c->domain); } catch (const std::exception& ex) { *e = err(ex.what()); return false; }
  return true;
}

bool parse_scenario(const Json& a, const std::string& domain, Scenario* sc, Json* e) {
  if (!a.has("scenario") || !a.at("scenario").is_object()) { *e = err("scenario (object) is required"); return false; }
  try { *sc = scenario_from_json(a.at("scenario"), domain); } catch (const std::exception& ex) { *e = err(std::string("bad scenario: ") + ex.what()); return false; }
  std::string why = validate(*sc);
  if (!why.empty()) { *e = err("invalid scenario: " + why); return false; }
  return true;
}

std::string slurp(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

Json op_domains() {
  Json out = Json::object();
  out["ok"] = true;
  Json ds = Json::array();
  for (const char* d : {"rigid2d", "orbit", "cartpole", "traffic"}) {
    Json o = Json::object();
    o["domain"] = d;
    Json rules = Json::array();
    for (const auto& r : rule_catalog(d)) { Json x = Json::object(); x["id"] = r.id; x["text"] = r.text; rules.push(x); }
    o["rules"] = rules;
    o["description"] = simulation_description(d);
    o["seeded_bugs"] = domain_num_bugs(d);
    ds.push(o);
  }
  out["domains"] = ds;
  return out;
}

Json op_run(const Json& a, const ApiOptions& opt) {
  Ctx c; Json e; Scenario sc;
  if (!make_ctx(a, opt, &c, &e) || !parse_scenario(a, c.domain, &sc, &e)) return e;
  Trace tr = c.sim(sc);
  std::vector<Violation> v = check_rules(sc, tr);
  Json out = Json::object();
  out["ok"] = true;
  out["steps"] = sc.steps;
  out["violation_count"] = static_cast<int>(v.size());
  Json vj = Json::array();
  for (const auto& x : v) vj.push(to_json(x));
  out["violations"] = vj;
  Json mj = Json::object();
  for (auto& kv : rule_margins(sc, tr)) mj[kv.first] = kv.second;
  out["rule_margins"] = mj;
  char h[32];
  std::snprintf(h, sizeof h, "%016llx", static_cast<unsigned long long>(tr.hash()));
  out["trace_hash"] = h;
  if (a.bool_or("include_signals", false)) {
    Json sj = Json::object();
    for (auto& kv : extract_signals(sc, tr)) { Json arr = Json::array(); for (double x : kv.second) arr.push(x); sj[kv.first] = arr; }
    out["signals"] = sj;
  }
  return out;
}

Json op_shrink(const Json& a, const ApiOptions& opt) {
  Ctx c; Json e; Scenario sc;
  if (!make_ctx(a, opt, &c, &e) || !parse_scenario(a, c.domain, &sc, &e)) return e;
  std::string rule = a.str_or("rule", "");
  if (rule.empty()) {
    auto v = check_rules(sc, c.sim(sc));
    if (v.empty()) return err("the scenario does not violate any rule on this engine; nothing to shrink");
    rule = v.front().rule;
  }
  ShrinkResult r = shrink(sc, c.sim, rule);
  if (!r.ok) return err("the scenario does not violate rule " + rule);
  Json out = Json::object();
  out["ok"] = true;
  out["rule"] = rule;
  out["scenario"] = to_json(r.scenario);
  out["violation"] = to_json(r.violation);
  out["size_before"] = r.size_before;
  out["size_after"] = r.size_after;
  out["attempts"] = r.attempts;
  return out;
}

Json op_bundle(const Json& a, const ApiOptions& opt) {
  Json s = op_shrink(a, opt);
  if (!s.bool_or("ok", false)) return s;
  char tmpl[] = "/tmp/rc_api_bundle_XXXXXX";
  if (!mkdtemp(tmpl)) return err("cannot create a temporary directory");
  std::string dir = tmpl;
  Json out = err("bundle failed");
  try {
    Scenario sc = scenario_from_json(s.at("scenario"), a.str_or("domain", "rigid2d"));
    Json exp = Json::object();
    exp["domain"] = a.str_or("domain", "rigid2d");
    exp["rule"] = s.str_or("rule", "");
    write_bundle(dir, sc, exp, "Produced by the RealityCheck API (shrunk).");
    out = Json::object();
    out["ok"] = true;
    Json files = Json::object();
    for (const char* f : {"scenario.json", "expected.json", "test.sh", "README.md"}) files[f] = slurp(dir + "/" + f);
    out["files"] = files;
  } catch (const std::exception& ex) { out = err(ex.what()); }
  if (std::system(("rm -rf '" + dir + "'").c_str()) != 0) {}
  return out;
}

Json op_hunt(const Json& a, const ApiOptions& opt) {
  Ctx c; Json e;
  if (!make_ctx(a, opt, &c, &e)) return e;
  Budget b;
  b.domain = c.domain;
  b.max_steps = static_cast<long long>(std::min(300000.0, std::max(1000.0, a.num_or("budget_steps", 100000))));
  b.max_wall_ms = 30000;
  HuntResult h = random_baseline(c.sim, static_cast<uint64_t>(a.num_or("seed", 1)), b);
  Json out = Json::object();
  out["ok"] = true;
  out["found"] = h.found;
  out["scenarios_run"] = h.scenarios_run;
  out["steps_used"] = static_cast<double>(h.steps_used);
  if (h.found) { out["scenario"] = to_json(h.first_failure); out["violation"] = to_json(h.first_violation); }
  return out;
}

Json op_random_scenario(const Json& a) {
  std::string d = a.str_or("domain", "rigid2d");
  if (!known_domain(d)) return err("unknown domain");
  Json out = Json::object();
  out["ok"] = true;
  out["scenario"] = to_json(random_scenario_for(d, static_cast<uint64_t>(a.num_or("seed", 1))));
  return out;
}

Json prop(const char* type, const char* desc) { Json p = Json::object(); p["type"] = type; p["description"] = desc; return p; }

Json tool(const char* name, const char* desc, Json props, std::vector<const char*> required) {
  Json t = Json::object();
  t["name"] = name; t["description"] = desc;
  Json s = Json::object();
  s["type"] = "object"; s["properties"] = props;
  Json req = Json::array();
  for (const char* r : required) req.push(r);
  s["required"] = req;
  t["inputSchema"] = s;
  return t;
}
}  // namespace

Json api_call(const std::string& op, const Json& args, const ApiOptions& opt) {
  try {
    if (op == "domains") return op_domains();
    if (op == "random_scenario") return op_random_scenario(args);
    if (op == "run") return op_run(args, opt);
    if (op == "shrink") return op_shrink(args, opt);
    if (op == "bundle") return op_bundle(args, opt);
    if (op == "hunt") return op_hunt(args, opt);
  } catch (const std::exception& ex) { return err(ex.what()); }
  return err("unknown operation: " + op);
}

Json mcp_tool_list() {
  Json common = Json::object();
  common["domain"] = prop("string", "rigid2d | orbit | cartpole | traffic");
  common["engine"] = prop("string", "builtin (clean reference, default), builtin:b02,b05 (seeded bugs), box2d, box2d-noccd");
  Json scen = prop("object", "A scenario of the chosen domain (see rc_domains for its limits). Use rc_random_scenario to get a valid example.");
  Json list = Json::array();
  list.push(tool("rc_domains", "List the simulation domains, their declared rules and scenario limits.", Json::object(), {}));
  Json rs = Json::object(); rs["domain"] = common.at("domain"); rs["seed"] = prop("integer", "Random seed.");
  list.push(tool("rc_random_scenario", "Draw a valid random scenario for a domain.", rs, {"domain"}));
  Json run = common; run["scenario"] = scen; run["include_signals"] = prop("boolean", "Also return per-step signal series.");
  list.push(tool("rc_run", "Simulate a scenario and check every declared rule at every step. The deterministic checker, not a model, decides pass or fail. Returns violations and rule margins.", run, {"domain", "scenario"}));
  Json sh = common; sh["scenario"] = scen; sh["rule"] = prop("string", "Rule id to preserve (default: the first violated rule).");
  list.push(tool("rc_shrink", "Reduce a failing scenario to a minimal one that still violates the same rule.", sh, {"domain", "scenario"}));
  list.push(tool("rc_bundle", "Shrink a failing scenario and return a regression-test bundle (scenario.json, expected.json, test.sh, README.md).", sh, {"domain", "scenario"}));
  Json hu = common; hu["seed"] = prop("integer", "Random seed."); hu["budget_steps"] = prop("integer", "Simulated-step budget (1000 to 300000).");
  list.push(tool("rc_hunt", "Random-testing hunt for a failing scenario within a step budget (no model, no cost).", hu, {"domain"}));
  return list;
}

}  // namespace rc
