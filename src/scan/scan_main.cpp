// rc_scan: point it at YOUR simulator, no model card to write and no API key. It probes the program for its signals, builds a model card from the
// parameter ranges you give, learns what "normal" looks like from the program's own runs (mined invariants), then hunts scenarios, including
// boundary values, for: failed runs, non-finite signals, nondeterminism, and runs that break a mined invariant. Each finding is minimized by
// resetting parameters to their defaults while it still fails, and written to a short report plus a card that every other tool accepts.
//
//   rc_scan --cmd ./mysim --param gain:0.1:8:log --param capacity:5:60:int --choice mode:proportional,bang_bang --steps 200 --out scan_out
//
// The program speaks the JSON-lines protocol of docs/PROTOCOL.md (generic domain: docs/GENERIC_DOMAIN.md). Mined invariants are candidates learned
// from the program itself: a bug present in every run looks normal to them, so a clean scan is not a certificate (docs/MINING.md).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include <unistd.h>

#include "external/engine.hpp"
#include "sim/domains.hpp"
#include "sim/generic.hpp"
#include "spec/miner.hpp"
#include "util/rng.hpp"

using namespace rc;

namespace {
std::vector<std::string> split(const std::string& s, char c) {
  std::vector<std::string> v;
  std::stringstream ss(s);
  for (std::string t; std::getline(ss, t, c);) v.push_back(t);
  return v;
}

// One probe request with default parameters; returns the signal names the program reports.
std::vector<std::string> probe_signals(const std::string& cmd, const Json& params, int steps) {
  Json sc = Json::object();
  sc["domain"] = "generic"; sc["seed"] = 1; sc["steps"] = steps; sc["params"] = params;
  Json req = Json::object();
  req["op"] = "simulate"; req["scenario"] = sc;
  const std::string in = "/tmp/rc_scan_probe_" + std::to_string(::getpid()) + ".json";
  std::ofstream(in) << req.dump() << "\n";
  FILE* p = popen(("( " + cmd + " ) < '" + in + "' 2>&1 | head -n 1").c_str(), "r");
  std::string line;
  if (p) { char buf[1 << 16]; if (fgets(buf, sizeof buf, p)) line = buf; pclose(p); }
  std::remove(in.c_str());
  if (line.empty()) throw std::runtime_error("the program printed nothing for the probe request; it must read one JSON line from stdin and answer with one (docs/PROTOCOL.md)");
  Json r;
  try { r = Json::parse(line); } catch (const std::exception&) { throw std::runtime_error("the program's first output line is not JSON: " + line.substr(0, 160)); }
  if (!r.has("ok") || !r.at("ok").boolean()) throw std::runtime_error("the program answered the probe with an error: " + line.substr(0, 200));
  std::vector<std::string> names;
  for (const auto& kv : r.at("signals").obj()) names.push_back(kv.first);
  if (names.empty()) throw std::runtime_error("the answer has no \"signals\" (name -> array of steps+1 numbers)");
  return names;
}

struct Finding {
  std::string kind, detail;
  int count = 0;
  Scenario example;
};

std::string describe(const GenericModel& m, const Scenario& sc) {
  std::ostringstream o;
  o << "steps=" << sc.steps;
  for (const GenericParam& g : m.params) {
    const double v = sc.gen.params.count(g.name) ? sc.gen.params.at(g.name) : g.def;
    if (std::fabs(v - g.def) < 1e-12 * (1 + std::fabs(g.def))) continue;
    if (g.type == "choice") o << ", " << g.name << "=" << g.choices[static_cast<size_t>(v)];
    else o << ", " << g.name << "=" << v;
  }
  return o.str();
}
}  // namespace

int main(int argc, char** argv) {
  std::string cmd, out = "scan_out", name = "my simulator";
  int steps = 200, n = 400, pool_n = 200;
  Json params = Json::array();
  std::vector<std::pair<std::string, Json>> defs;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--cmd") cmd = val(); else if (a == "--out") out = val(); else if (a == "--name") name = val();
    else if (a == "--steps") steps = std::atoi(val().c_str()); else if (a == "--scenarios") n = std::atoi(val().c_str());
    else if (a == "--param" || a == "--choice") {
      const std::vector<std::string> f = split(val(), a == "--param" ? ':' : ':');
      Json p = Json::object();
      p["name"] = f.at(0);
      if (a == "--choice") {
        Json ch = Json::array();
        for (const std::string& c : split(f.at(1), ',')) ch.push(c);
        p["type"] = "choice"; p["choices"] = ch; p["default"] = split(f.at(1), ',').at(0);
      } else {
        if (f.size() < 3) { std::cerr << "--param NAME:MIN:MAX[:int|log]\n"; return 2; }
        const double lo = std::atof(f[1].c_str()), hi = std::atof(f[2].c_str());
        const bool is_int = f.size() > 3 && f[3] == "int", is_log = f.size() > 3 && f[3] == "log";
        p["type"] = is_int ? "int" : "float"; p["min"] = lo; p["max"] = hi;
        if (is_log) { p["scale"] = "log"; p["default"] = std::sqrt(lo * hi); } else p["default"] = is_int ? std::floor((lo + hi) / 2) : (lo + hi) / 2;
      }
      params.push(p);
    } else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  if (cmd.empty() || params.arr().empty()) {
    std::cerr << "usage: rc_scan --cmd PROGRAM --param NAME:MIN:MAX[:int|log] [--param ...] [--choice NAME:a,b,c] [--steps N] [--scenarios N] [--out DIR] [--name TEXT]\n";
    return 2;
  }
  try {
    Json defaults = Json::object();
    for (const Json& p : params.arr()) defaults[p.at("name").str()] = p.at("default");
    const std::vector<std::string> sigs = probe_signals(cmd, defaults, std::min(steps, 20));
    Json card = Json::object();
    card["name"] = name;
    card["description"] = "Simulator under scan: " + name + ". Signals: " + [&] { std::string s; for (const auto& x : sigs) s += (s.empty() ? "" : ", ") + x; return s; }();
    card["command"] = cmd;
    card["parameters"] = params;
    Json st = Json::object(); st["min"] = std::min(steps, 20); st["max"] = steps; st["default"] = steps; card["steps"] = st;
    Json sj = Json::array();
    for (const std::string& s : sigs) { Json o = Json::object(); o["name"] = s; o["text"] = s; sj.push(o); }
    card["signals"] = sj;
    card["rules"] = Json::array();
    set_generic_model(parse_generic_model(card));
    const GenericModel& m = generic_model();
    std::system(("mkdir -p '" + out + "'").c_str());
    std::ofstream(out + "/card.json") << card.dump() << "\n";
    std::printf("probed %s: %zu signals (%s), %zu parameters\n", cmd.c_str(), sigs.size(), card.at("description").str().substr(card.at("description").str().find("Signals: ") + 9).c_str(), m.params.size());

    SimFn sim = make_engine("card", "generic");
    auto run = [&](const Scenario& sc, Signals* sg, std::string* err) {
      try { Trace tr = sim(sc); *sg = extract_signals(sc, tr); return true; }
      catch (const std::exception& e) { *err = e.what(); return false; }
    };
    // Boundary-aware scenario: each parameter is pushed to its range edge with probability 0.3 (docs/HYBRID.md: seeded bugs often trigger at edges).
    auto scenario = [&](uint64_t seed, bool edges) {
      Scenario sc = random_generic_scenario(seed);
      sc.steps = std::min(sc.steps, steps);
      if (edges) {
        Rng r(seed * 7919 + 13);
        for (const GenericParam& g : m.params) {
          if (g.type == "choice" || r.range(0, 1) > 0.3) continue;
          sc.gen.params[g.name] = r.range(0, 1) < 0.5 ? g.min : g.max;
        }
      }
      return sc;
    };

    std::vector<Signals> pool;
    int pool_failed = 0;
    for (int i = 0; i < pool_n; ++i) {
      Signals sg; std::string err;
      if (run(scenario(900000 + static_cast<uint64_t>(i), false), &sg, &err)) pool.push_back(sg); else ++pool_failed;
    }
    if (pool.empty()) throw std::runtime_error("every probe run failed; check the program on a default scenario");
    std::vector<Invariant> inv = mine_invariants(pool);
    std::printf("learned %zu candidate invariants from %zu normal runs\n", inv.size(), pool.size());

    std::map<std::string, Finding> found;
    auto record = [&](const std::string& key, const std::string& kind, const std::string& detail, const Scenario& sc) {
      Finding& f = found[key];
      if (f.count++ == 0) { f.kind = kind; f.detail = detail; f.example = sc; }
    };
    auto classify = [&](const Scenario& sc, std::vector<std::pair<std::string, std::string>>* hits) {
      Signals sg, sg2; std::string err;
      if (!run(sc, &sg, &err)) { hits->push_back({"run failed", "the program failed or answered an error: " + err.substr(0, 160)}); return; }
      for (const auto& kv : sg)
        for (size_t k = 0; k < kv.second.size(); ++k)
          if (!std::isfinite(kv.second[k])) { hits->push_back({"non-finite signal " + kv.first, "signal " + kv.first + " is not finite at step " + std::to_string(k)}); goto done_nan; }
      done_nan:
      if (run(sc, &sg2, &err) && sg2 != sg) {
        bool same = true;
        for (const auto& kv : sg) { const auto& o = sg2.at(kv.first); for (size_t k = 0; k < kv.second.size() && same; ++k) if (!(kv.second[k] == o[k] || (std::isnan(kv.second[k]) && std::isnan(o[k])))) same = false; }
        if (!same) hits->push_back({"nondeterministic", "the same scenario gives different signals on a second run"});
      }
      for (const Invariant& v : inv) { InvFail f = check_invariant(v, sg); if (f.failed) hits->push_back({"breaks learned invariant: " + v.text, v.text + " (observed " + std::to_string(f.observed) + " vs limit " + std::to_string(f.limit) + " at step " + std::to_string(f.step) + ")"}); }
    };
    for (int i = 0; i < n; ++i) {
      Scenario sc = scenario(700000 + static_cast<uint64_t>(i), i % 2 == 1);
      std::vector<std::pair<std::string, std::string>> hits;
      classify(sc, &hits);
      for (const auto& h : hits) record(h.first, h.first.substr(0, 3) == "run" ? "failure" : h.first.substr(0, 3) == "non" ? "numeric" : h.first.substr(0, 3) == "bre" ? "invariant" : "determinism", h.second, sc);
    }
    // Minimize each finding: reset parameters to their defaults while the same finding still appears.
    for (auto& kv : found) {
      Scenario sc = kv.second.example;
      auto still = [&](const Scenario& s) { std::vector<std::pair<std::string, std::string>> h; classify(s, &h); for (const auto& x : h) if (x.first == kv.first) return true; return false; };
      for (const GenericParam& g : m.params) { Scenario t = sc; t.gen.params[g.name] = g.def; if (still(t)) sc = t; }
      { Scenario t = sc; t.steps = std::max(m.steps_min, sc.steps / 2); if (t.steps != sc.steps && still(t)) sc = t; }
      kv.second.example = sc;
    }

    std::ostringstream rep;
    rep << "# Scan of " << name << "\n\n" << n << " scenarios (half pushed to parameter boundaries), " << pool.size() << " normal runs mined, " << inv.size() << " candidate invariants.\n\n";
    if (found.empty()) rep << "No finding. This is not a certificate: a bug present in every run looks normal to learned invariants (docs/MINING.md). Add your own rules to `card.json` (docs/GENERIC_DOMAIN.md) or review the learned invariants.\n";
    else rep << "| finding | scenarios | smallest example (non-default values) |\n|---|---|---|\n";
    for (const auto& kv : found) rep << "| " << kv.first << " | " << kv.second.count << " of " << n << " | " << describe(m, kv.second.example) << " |\n";
    rep << "\nRe-run with the saved card: `RC_MODEL_CARD=" << out << "/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.\n"
        << "Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.\n";
    std::ofstream(out + "/report.md") << rep.str();
    std::printf("\n%s\nreport: %s/report.md   card: %s/card.json\n", rep.str().c_str(), out.c_str(), out.c_str());
    return found.empty() ? 0 : 1;
  } catch (const std::exception& e) {
    std::cerr << "rc_scan: " << e.what() << "\n";
    return 2;
  }
}
