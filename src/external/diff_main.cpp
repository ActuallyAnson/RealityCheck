// rc_diff: differential testing of two engines on random scenarios (no rules, no model, no API).
//   rc_diff --a SPEC --b SPEC [--domain D] [--scenarios N] [--seed S] [--tol T] [--horizon S] [--out FILE] [--bundles DIR]
// A scenario is flagged when the engines' body positions differ by more than T at some step. The first flagged
// scenario is shrunk (the shrinker keeps "the engines still disagree") and optionally saved as a regression bundle
// that `rc_repro check --engine A --engine-b B` replays.
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "baseline/random_baseline.hpp"
#include "external/bundle.hpp"
#include "external/diverge.hpp"
#include "external/engine.hpp"
#include "shrink/shrink.hpp"
#include "sim/cartpole.hpp"
#include "sim/orbit.hpp"
#include "util/rng.hpp"

using namespace rc;

int main(int argc, char** argv) {
  int n = 500;
  uint64_t seed = 1;
  double tol = 0.05, horizon = 1e30;
  std::string a_spec = "builtin", b_spec, domain = "rigid2d", out, bundles;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
    if (a == "--a") a_spec = val();
    else if (a == "--b") b_spec = val();
    else if (a == "--domain") domain = val();
    else if (a == "--scenarios") n = std::atoi(val().c_str());
    else if (a == "--seed") seed = std::strtoull(val().c_str(), nullptr, 10);
    else if (a == "--tol") tol = std::atof(val().c_str());
    else if (a == "--horizon") horizon = std::atof(val().c_str());  // seconds compared (chaotic systems amplify tiny differences)
    else if (a == "--out") out = val();
    else if (a == "--bundles") bundles = val();
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  if (b_spec.empty()) { std::cerr << "usage: rc_diff --a SPEC --b SPEC [--domain D] [--scenarios N] [--seed S] [--tol T]\n"; return 2; }
  SimFn A, B;
  try { A = make_engine(a_spec, domain); B = make_engine(b_spec, domain); }
  catch (const std::exception& e) { std::cerr << e.what() << "\n"; return 2; }
  auto disagree = [&](const Scenario& s, Violation* v) { return first_divergence(s, A(s), B(s), tol, v, horizon); };
  Rng rng(seed);
  int flagged = 0, first_idx = 0;
  Scenario first;
  std::vector<int> steps_of_first;
  try {
    for (int i = 1; i <= n; ++i) {
      Scenario s = domain == "orbit" ? random_orbit_scenario(rng.next())
                   : domain == "cartpole" ? random_cartpole_scenario(rng.next()) : random_scenario(rng.next());
      if (domain == "rigid2d") { s.ctrl.enabled = false; s.ctrl.waypoints.clear(); }
      if (disagree(s, nullptr)) { if (!flagged++) { first = s; first_idx = i; } }
    }
  } catch (const std::exception& e) { std::cerr << "engine error: " << e.what() << "\n"; return 2; }
  std::cout << "A=" << a_spec << "  B=" << b_spec << "  domain=" << domain << "  tol=" << tol << "\n"
            << flagged << "/" << n << " scenarios where the engines disagree";
  Json report = Json::object();
  report["a"] = a_spec; report["b"] = b_spec; report["domain"] = domain; report["tolerance"] = tol; report["horizon_s"] = horizon;
  report["scenarios"] = n; report["disagreements"] = flagged;
  if (flagged) {
    ShrinkResult sr = shrink(first, FailFn(disagree));
    std::cout << " (first at #" << first_idx << ", shrunk " << sr.size_before << " -> " << sr.size_after << ")";
    report["first_at_scenario"] = first_idx;
    if (sr.ok) {
      report["shrunk"] = to_json(sr.scenario);
      report["detail"] = sr.violation.detail;
      std::cout << "\n  shrunk case: " << sr.violation.detail;
      if (!bundles.empty()) {
        Json e = Json::object();
        e["domain"] = domain; e["rule"] = "engines_diverge"; e["kind"] = "differential"; e["tolerance"] = tol; e["horizon"] = horizon;
        e["engine_a"] = a_spec; e["engine_b"] = b_spec; e["detail"] = sr.violation.detail;
        write_bundle(bundles + "/" + domain + "_diff", sr.scenario, e, "Found by rc_diff: engines '" + a_spec + "' and '" + b_spec + "' disagree.");
      }
    }
  }
  std::cout << "\n";
  if (!out.empty()) std::ofstream(out) << report.dump() << "\n";
  return 0;
}
