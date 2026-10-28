// rc_repro: turn a found failure into a standalone regression test, and run such a test against any engine.
//   rc_repro export --run RUN_DIR --out DIR [--domain D]
//       RUN_DIR holds report.json from rc_agent. Writes DIR/{scenario.json, expected.json, test.sh, README.md}.
//   rc_repro check --repro DIR --engine SPEC [--domain D] [--junit FILE]
//       Runs the scenario through the engine and checks the rule. Exit 0 = rule holds (bug fixed / absent),
//       1 = the failure reproduces, 2 = error. SPEC: see src/external/engine.hpp.
//       Differential bundles (from rc_diff) also need --engine-b SPEC.
#include <sys/stat.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "eval/ground_truth.hpp"
#include "external/bundle.hpp"
#include "external/diverge.hpp"
#include "external/engine.hpp"
#include "rules/checker.hpp"

using namespace rc;

namespace {

int do_export(const std::string& run, const std::string& out, std::string domain) {
  Json rep = Json::parse(read_file(run + "/report.json"));
  if (!rep.has("minimal_scenario") || !rep.has("rule")) { std::cerr << "report.json has no minimal_scenario/rule\n"; return 2; }
  Json sc = rep.at("minimal_scenario");
  if (sc.has("domain")) domain = sc.str_or("domain", domain);
  Scenario s = scenario_from_json(sc, domain);
  if (!validate(s).empty()) { std::cerr << "scenario invalid: " << validate(s) << "\n"; return 2; }
  std::string rule = rep.at("rule").str();
  // Re-run on the clean built-in engine only to confirm the file is a well-formed scenario; the expected values
  // come from the report's own violation (the engine that found it), not from the clean simulator.
  Json exp = Json::object();
  exp["domain"] = domain;
  exp["rule"] = rule;
  exp["found_by"] = "RealityCheck agent report " + run;
  exp["explanation"] = rep.str_or("explanation", "");
  if (rep.has("citations")) exp["citations"] = rep.at("citations");
  write_bundle(out, s, exp, "Found by a RealityCheck agent run.");
  std::cout << "wrote regression test to " << out << " (rule " << rule << ", domain " << domain << ")\n";
  return 0;
}

int do_check(const std::string& dir, const std::string& engine, const std::string& engine_b, std::string domain, const std::string& junit) {
  Json exp = Json::parse(read_file(dir + "/expected.json"));
  if (domain.empty()) domain = exp.str_or("domain", "rigid2d");
  std::string rule = exp.at("rule").str();
  Scenario sc = scenario_from_json(Json::parse(read_file(dir + "/scenario.json")), domain);
  std::string why = validate(sc);
  if (!why.empty()) { std::cerr << "scenario invalid: " << why << "\n"; return 2; }
  SimFn sim = make_engine(engine, domain);
  Trace tr = sim(sc);
  Violation diff;
  const Violation* hit = nullptr;
  std::vector<Violation> all;
  if (exp.str_or("kind", "") == "differential") {  // two engines must agree within tolerance
    if (engine_b.empty()) { std::cerr << "a differential test needs --engine-b\n"; return 2; }
    if (first_divergence(sc, tr, make_engine(engine_b, domain)(sc), exp.num_or("tolerance", 0.05), &diff, exp.num_or("horizon", 1e30))) hit = &diff;
  } else {
    all = check_rules(sc, tr);
    for (const auto& v : all) if (v.rule == rule) { hit = &v; break; }
  }
  if (hit) std::cout << "FAIL " << rule << " at step " << hit->step << ": " << hit->detail << " [engine " << engine << "]\n";
  else std::cout << "PASS " << rule << " holds [engine " << engine << "]\n";
  if (!junit.empty()) {
    std::ostringstream x;
    x << "<?xml version=\"1.0\"?>\n<testsuite name=\"realitycheck\" tests=\"1\" failures=\"" << (hit ? 1 : 0) << "\">\n"
      << "  <testcase classname=\"" << domain << "\" name=\"" << rule << "\">\n";
    if (hit) x << "    <failure message=\"" << rule << " violated at step " << hit->step << "\"/>\n";
    x << "  </testcase>\n</testsuite>\n";
    write_text(junit, x.str());
  }
  return hit ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::cerr << "usage: rc_repro export|check ...\n"; return 2; }
  std::string cmd = argv[1], run, out, repro, engine = "builtin", engine_b, domain, junit;
  for (int i = 2; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
    if (a == "--run") run = val();
    else if (a == "--out") out = val();
    else if (a == "--repro") repro = val();
    else if (a == "--engine") engine = val();
    else if (a == "--engine-b") engine_b = val();
    else if (a == "--domain") domain = val();
    else if (a == "--junit") junit = val();
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  try {
    if (cmd == "export") return do_export(run, out, domain.empty() ? "rigid2d" : domain);
    if (cmd == "check") return do_check(repro, engine, engine_b, domain, junit);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 2;
  }
  std::cerr << "unknown command " << cmd << "\n";
  return 2;
}
