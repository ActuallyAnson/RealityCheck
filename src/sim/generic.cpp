#include "sim/generic.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>

#include "util/rng.hpp"

namespace rc {
namespace {
std::mutex g_mu;
GenericModel g_model;
bool g_loaded = false;

std::string slurp(const std::string& p) {
  std::ifstream f(p);
  if (!f) throw std::runtime_error("cannot read the model card " + p);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
}  // namespace

GenericModel parse_generic_model(const Json& c) {
  GenericModel m;
  m.name = c.str_or("name", "");
  m.description = c.str_or("description", "");
  m.command = c.str_or("command", "");
  if (m.name.empty()) throw std::runtime_error("model card: \"name\" is required");
  if (m.description.empty()) throw std::runtime_error("model card: \"description\" is required (the model reads it)");
  if (!c.has("parameters") || !c.at("parameters").is_array() || c.at("parameters").arr().empty()) throw std::runtime_error("model card: \"parameters\" must be a non-empty array");
  for (const Json& p : c.at("parameters").arr()) {
    GenericParam g;
    g.name = p.str_or("name", "");
    g.type = p.str_or("type", "float");
    g.text = p.str_or("text", "");
    if (g.name.empty()) throw std::runtime_error("model card: a parameter has no name");
    if (g.type == "choice") {
      if (!p.has("choices") || !p.at("choices").is_array() || p.at("choices").arr().size() < 2) throw std::runtime_error("model card: parameter " + g.name + ": choices needs at least 2 strings");
      for (const Json& x : p.at("choices").arr()) g.choices.push_back(x.str());
      g.min = 0; g.max = static_cast<double>(g.choices.size() - 1);
      std::string d = p.str_or("default", g.choices[0]);
      auto it = std::find(g.choices.begin(), g.choices.end(), d);
      if (it == g.choices.end()) throw std::runtime_error("model card: parameter " + g.name + ": default is not one of the choices");
      g.def = static_cast<double>(it - g.choices.begin());
    } else if (g.type == "float" || g.type == "int") {
      if (!p.has("min") || !p.has("max")) throw std::runtime_error("model card: parameter " + g.name + ": min and max are required");
      g.min = p.at("min").num(); g.max = p.at("max").num();
      if (!(g.min <= g.max) || !std::isfinite(g.min) || !std::isfinite(g.max)) throw std::runtime_error("model card: parameter " + g.name + ": bad range");
      g.def = p.num_or("default", g.min);
      if (g.def < g.min || g.def > g.max) throw std::runtime_error("model card: parameter " + g.name + ": default outside the range");
      g.log_scale = p.str_or("scale", "linear") == "log" && g.min > 0;
    } else throw std::runtime_error("model card: parameter " + g.name + ": type must be float, int or choice");
    for (const GenericParam& q : m.params) if (q.name == g.name) throw std::runtime_error("model card: duplicate parameter " + g.name);
    m.params.push_back(g);
  }
  if (c.has("steps")) { m.steps_min = static_cast<int>(c.at("steps").num_or("min", 10)); m.steps_max = static_cast<int>(c.at("steps").num_or("max", 1000)); m.steps_def = static_cast<int>(c.at("steps").num_or("default", 200)); }
  if (m.steps_min < 1 || m.steps_max < m.steps_min || m.steps_max > 100000 || m.steps_def < m.steps_min || m.steps_def > m.steps_max) throw std::runtime_error("model card: bad steps range (1 <= min <= default <= max <= 100000)");
  if (!c.has("signals") || !c.at("signals").is_array() || c.at("signals").arr().empty()) throw std::runtime_error("model card: \"signals\" must be a non-empty array");
  for (const Json& s : c.at("signals").arr()) {
    SignalInfo si{s.str_or("name", ""), s.str_or("text", "")};
    if (si.name.empty()) throw std::runtime_error("model card: a signal has no name");
    for (const SignalInfo& o : m.signals) if (o.name == si.name) throw std::runtime_error("model card: duplicate signal " + si.name);
    m.signals.push_back(si);
  }
  // Rules are validated against the signal list by the invariant parser, which reads the current generic model: install this one temporarily.
  struct Restore {
    GenericModel saved; bool loaded;
    Restore() { std::lock_guard<std::mutex> g(g_mu); saved = g_model; loaded = g_loaded; }
    ~Restore() { std::lock_guard<std::mutex> g(g_mu); g_model = saved; g_loaded = loaded; }
  } restore;
  set_generic_model(m);
  if (c.has("rules")) {
    for (const Json& r : c.at("rules").arr()) {
      std::vector<std::string> pr;
      auto v = parse_invariants("[" + r.dump() + "]", "generic", &pr, 2);
      if (v.size() != 1) throw std::runtime_error("model card: rule " + r.str_or("id", "?") + " is invalid: " + (pr.empty() ? "unknown" : pr[0]));
      GenericRule gr;
      gr.id = r.str_or("id", v[0].id);
      gr.text = r.str_or("text", "");
      gr.inv = v[0];
      gr.inv.id = gr.id;
      m.rules.push_back(gr);
    }
  }
  return m;
}

void set_generic_model(const GenericModel& m) {
  std::lock_guard<std::mutex> g(g_mu);
  g_model = m;
  g_loaded = true;
}

const GenericModel& generic_model() {
  {
    std::lock_guard<std::mutex> g(g_mu);
    if (g_loaded) return g_model;
  }
  const char* p = std::getenv("RC_MODEL_CARD");
  if (!p || !*p) throw std::runtime_error("the generic domain needs a model card: set RC_MODEL_CARD=path/to/card.json (see docs/GENERIC_DOMAIN.md)");
  GenericModel m = parse_generic_model(Json::parse(slurp(p)));
  set_generic_model(m);
  std::lock_guard<std::mutex> g(g_mu);
  return g_model;
}

std::string generic_validate(const Scenario& s) {
  const GenericModel& m = generic_model();
  if (s.steps < m.steps_min || s.steps > m.steps_max) return "steps outside [" + std::to_string(m.steps_min) + ", " + std::to_string(m.steps_max) + "]";
  for (const auto& kv : s.gen.params) {
    bool known = false;
    for (const GenericParam& p : m.params) known |= p.name == kv.first;
    if (!known) return "unknown parameter " + kv.first;
  }
  for (const GenericParam& p : m.params) {
    auto it = s.gen.params.find(p.name);
    if (it == s.gen.params.end()) return "missing parameter " + p.name;
    double v = it->second;
    if (!std::isfinite(v) || v < p.min || v > p.max) return "parameter " + p.name + " outside its range";
    if ((p.type == "int" || p.type == "choice") && v != std::floor(v)) return "parameter " + p.name + " must be an integer";
  }
  return "";
}

Json generic_to_json(const Scenario& s) {
  const GenericModel& m = generic_model();
  Json j = Json::object();
  j["domain"] = "generic";
  j["seed"] = static_cast<double>(s.seed);
  j["steps"] = s.steps;
  Json p = Json::object();
  for (const GenericParam& g : m.params) {
    auto it = s.gen.params.find(g.name);
    double v = it == s.gen.params.end() ? g.def : it->second;
    if (g.type == "choice") p[g.name] = g.choices[static_cast<size_t>(std::max(0.0, std::min(v, static_cast<double>(g.choices.size() - 1))))];
    else if (g.type == "int") p[g.name] = std::floor(v);
    else p[g.name] = v;
  }
  j["params"] = p;
  return j;
}

Scenario generic_from_json(const Json& j) {
  const GenericModel& m = generic_model();
  Scenario s;
  s.domain = "generic";
  s.seed = clamp_u64(j.num_or("seed", 1));
  s.steps = clamp_int(j.num_or("steps", m.steps_def));
  s.dt = 1;
  for (const GenericParam& g : m.params) s.gen.params[g.name] = g.def;
  if (j.has("params")) {
    if (!j.at("params").is_object()) throw std::runtime_error("params must be an object");
    for (const auto& kv : j.at("params").obj()) {
      const GenericParam* g = nullptr;
      for (const GenericParam& p : m.params) if (p.name == kv.first) g = &p;
      if (!g) { s.gen.params[kv.first] = kv.second.is_number() ? kv.second.num() : 0; continue; }  // unknown names are kept so validate() can reject them
      if (g->type == "choice") {
        if (kv.second.is_string()) {
          auto it = std::find(g->choices.begin(), g->choices.end(), kv.second.str());
          s.gen.params[g->name] = it == g->choices.end() ? -1 : static_cast<double>(it - g->choices.begin());
        } else s.gen.params[g->name] = kv.second.is_number() ? kv.second.num() : -1;
      } else {
        if (!kv.second.is_number()) throw std::runtime_error("parameter " + g->name + " must be a number");
        s.gen.params[g->name] = kv.second.num();
      }
    }
  }
  return s;
}

Scenario random_generic_scenario(uint64_t seed) {
  const GenericModel& m = generic_model();
  Rng rng(seed);
  Scenario s;
  s.domain = "generic";
  s.seed = rng.next() % 1000000;
  s.dt = 1;
  s.steps = rng.irange(m.steps_min, m.steps_max);
  for (const GenericParam& g : m.params) {
    double v;
    if (g.type == "choice") v = rng.irange(0, static_cast<int>(g.choices.size()) - 1);
    else if (g.type == "int") v = rng.irange(static_cast<int>(g.min), static_cast<int>(g.max));
    else if (g.log_scale) v = std::exp(rng.range(std::log(g.min), std::log(std::max(g.max, g.min * (1 + 1e-12)))));
    else v = rng.range(g.min, g.max);
    s.gen.params[g.name] = std::min(g.max, std::max(g.min, v));
  }
  return s;
}

Signals generic_signals(const Scenario&, const Trace& tr) {
  const GenericModel& m = generic_model();
  Signals out;
  for (size_t i = 0; i < m.signals.size(); ++i) {
    std::vector<double>& v = out[m.signals[i].name];
    for (const StepRecord& r : tr.rec) v.push_back(i < r.body.size() ? r.body[i].x : std::nan(""));
  }
  return out;
}

std::vector<Violation> check_generic_rules(const Scenario& sc, const Trace& tr) {
  const GenericModel& m = generic_model();
  std::vector<Violation> out;
  Signals sig = generic_signals(sc, tr);
  {
    int first = -1;
    std::string which;
    for (const auto& kv : sig)
      for (size_t k = 0; k < kv.second.size(); ++k)
        if (!std::isfinite(kv.second[k]) && (first < 0 || static_cast<int>(k) < first)) { first = static_cast<int>(k); which = kv.first; }
    if (first >= 0) out.push_back({"no_nan", first, -1, 0, 0, "signal " + which + " is not finite"});
  }
  for (const GenericRule& r : m.rules) {
    InvFail f = check_invariant(r.inv, sig);
    if (f.failed) out.push_back({r.id, f.step, -1, f.observed, f.limit, r.text.empty() ? r.id : r.text});
  }
  std::stable_sort(out.begin(), out.end(), [](const Violation& a, const Violation& b) { return a.step != b.step ? a.step < b.step : a.rule < b.rule; });
  return out;
}

std::string generic_scenario_text() {
  const GenericModel& m = generic_model();
  std::ostringstream o;
  o << "Scenario: {seed, steps, params:{";
  for (size_t i = 0; i < m.params.size(); ++i) o << (i ? ", " : "") << m.params[i].name;
  o << "}}. Limits: steps in [" << m.steps_min << ", " << m.steps_max << "]";
  for (const GenericParam& p : m.params) {
    o << "; " << p.name << " ";
    if (p.type == "choice") { o << "one of"; for (const auto& c : p.choices) o << " \"" << c << "\""; }
    else { o << (p.type == "int" ? "integer in [" : "in ["); o << p.min << ", " << p.max << "]"; }
    if (!p.text.empty()) o << " (" << p.text << ")";
  }
  o << ".";
  return o.str();
}

std::string generic_description() {
  const GenericModel& m = generic_model();
  std::ostringstream o;
  o << "SIMULATION UNDER TEST\n" << m.name << "\n" << m.description << "\n\nSIGNALS recorded at every step (step 0 is the initial state):\n";
  for (const SignalInfo& s : m.signals) o << "- " << s.name << ": " << s.text << "\n";
  o << "\nSCENARIO LIMITS\n" << generic_scenario_text() << "\n\nDECLARED RULES (checked at every step)\n";
  for (const RuleInfo& r : generic_rule_catalog()) o << "- " << r.id << ": " << r.text << "\n";
  o << "\nThe rule checker, not you, decides what is a violation.\nIn submit_report citations, set `field` to one of the signal names above (and body to -1) and `value` to the signal's value at that step.\n";
  return o.str();
}

const std::vector<RuleInfo>& generic_rule_catalog() {
  static std::vector<RuleInfo> cat;
  static std::deque<std::string> store;
  static const GenericModel* built_for = nullptr;
  const GenericModel& m = generic_model();
  if (built_for != &m || cat.size() != m.rules.size() + 1) {
    cat.clear(); store.clear();
    cat.push_back({"no_nan", "Every signal value stays finite."});
    for (const GenericRule& r : m.rules) {
      store.push_back(r.id); store.push_back(r.text.empty() ? r.id : r.text);
      cat.push_back({store[store.size() - 2].c_str(), store.back().c_str()});
    }
    built_for = &m;
  }
  return cat;
}

}  // namespace rc
