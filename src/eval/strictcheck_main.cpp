// rc_strictcheck: replays recorded agent sessions (events.jsonl) through the deterministic tools and re-validates each final report with the default
// and the strict validator, then applies forgeries that only the strict mode can refuse. No model, no network.
//   rc_strictcheck RUNS_DIR...      (each RUNS_DIR holds <name>.json and <name>/{events.jsonl,report.json}, as written by rc_triage)
#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "agent/agent.hpp"
#include "eval/ground_truth.hpp"
#include "sim/domains.hpp"
#include "tools/tools.hpp"

using namespace rc;

namespace {
bool read_file(const std::string& p, std::string* out) {
  std::ifstream f(p);
  if (!f) return false;
  std::stringstream ss; ss << f.rdbuf(); *out = ss.str();
  return true;
}
void collect(const Json& j, std::vector<double>* out) {
  if (j.is_number()) out->push_back(j.num());
  else if (j.is_array()) for (const Json& e : j.arr()) collect(e, out);
  else if (j.is_object()) for (const auto& kv : j.obj()) collect(kv.second, out);
}
struct Tally { int n = 0, accepted = 0; };
}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> dirs;
  StrictOptions opt;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--rel" && i + 1 < argc) opt.rel_tol = std::atof(argv[++i]);
    else if (a == "--derived") opt.derived = true;
    else if (a == "--prose-rel" && i + 1 < argc) opt.prose_rel = std::atof(argv[++i]);
    else dirs.push_back(a);
  }
  int total = 0, replayed = 0, honest_default = 0, honest_strict = 0, fail_cite = 0, fail_prose = 0, fail_cause = 0;
  Tally u1, u2, u4, u3;
  std::vector<std::string> rejected_examples;
  for (const std::string& dir : dirs) {
    std::set<std::string> names;
    std::string cmd = "ls " + dir + " | sed -n 's/\\.json$//p'";
    FILE* p = popen(cmd.c_str(), "r");
    char buf[512];
    while (p && std::fgets(buf, sizeof buf, p)) { std::string n = buf; while (!n.empty() && (n.back() == '\n')) n.pop_back(); names.insert(n); }
    if (p) pclose(p);
    for (const std::string& name : names) {
      std::string meta_t, rep_t, ev_t;
      if (!read_file(dir + "/" + name + ".json", &meta_t) || !read_file(dir + "/" + name + "/report.json", &rep_t) || !read_file(dir + "/" + name + "/events.jsonl", &ev_t)) continue;
      Json meta = Json::parse(meta_t), rep = Json::parse(rep_t);
      if (!rep.bool_or("report_valid", false)) continue;
      ++total;
      std::string domain = meta.str_or("domain", "rigid2d"), bug = meta.str_or("bug", "");
      int idx = domain_bug_index(domain, bug);
      if (idx < 0) continue;
      ToolSession ts(static_cast<BugMask>(1) << idx, ToolBudget{100000000, 100000}, "");
      ts.set_domain(domain);
      // The fuzzer's seed run is not in the event log, so rebuild the two scenarios the report depends on: the shrunk scenario (stored in report.json)
      // and the counterfactual (its run_scenario call is in the log). Scenario ids are content hashes, so they match the original session.
      { Json a = Json::object(); a["scenario"] = rep.at("minimal_scenario"); ts.call("run_scenario", a); }
      std::vector<double> seen;
      std::istringstream es(ev_t);
      std::string line;
      while (std::getline(es, line)) {
        if (line.empty()) continue;
        Json e = Json::parse(line);
        if (e.str_or("type", "") == "tool_result" && e.has("result")) collect(e.at("result"), &seen);
        if (e.str_or("type", "") == "tool_call" && e.str_or("tool", "") == "run_scenario" && e.at("args").has("scenario") && e.at("args").at("scenario").is_object())
          ts.call("run_scenario", e.at("args"));
      }
      Json shrunk_args = Json::object(); (void)shrunk_args;
      std::string id = rep.str_or("scenario_id", "");
      const ToolSession::Entry* run = ts.record(id);
      if (!run) continue;
      ++replayed;
      const ToolSession::Entry* cf = nullptr;
      if (rep.has("counterfactual") && rep.at("counterfactual").is_object()) cf = ts.record(rep.at("counterfactual").str_or("scenario_id", ""));
      // the stored report.json is the accepted report: citations/explanation/rule/counterfactual
      Json r = rep;
      opt.seen = &seen;
      CitationCheck d = validate_report(*run, true, r);
      CitationCheck s = validate_report_strict(*run, cf, true, r, opt);
      honest_default += d.ok;
      honest_strict += s.ok;
      if (d.ok && !s.ok) {
        bool c1 = false, c2 = false, c3 = false;
        for (const std::string& m : s.problems) { if (m.rfind("strict citation", 0) == 0) c1 = true; if (m.rfind("strict prose", 0) == 0) c2 = true; if (m.rfind("strict cause", 0) == 0) c3 = true; }
        fail_cite += c1; fail_prose += c2; fail_cause += c3;
        for (const std::string& m : s.problems) if (m.rfind("strict", 0) == 0 && rejected_examples.size() < 12) rejected_examples.push_back(name + ": " + m);
      }
      if (!d.ok) continue;
      // forgeries (only applied to reports the default validator accepts)
      auto field_cits = [&](Json rr, double scale) {  // multiply every cited value
        Json cs = Json::array();
        for (Json c : rr.at("citations").arr()) { if (c.has("value") && c.at("value").is_number()) c["value"] = c.at("value").num() * scale; cs.push(c); }
        rr["citations"] = cs; return rr; };
      { Json f = field_cits(r, 1.008); if (validate_report(*run, true, f).ok) { ++u1.n; u1.accepted += validate_report_strict(*run, cf, true, f, opt).ok; } }
      { Json f = r; Json cs = Json::array(); for (Json c : r.at("citations").arr()) { Json c2 = Json::object(); c2["step"] = c.at("step").num(); c2["body"] = c.num_or("body", 0); cs.push(c2); } f["citations"] = cs;
        if (validate_report(*run, true, f).ok) { ++u2.n; u2.accepted += validate_report_strict(*run, cf, true, f, opt).ok; } }
      { Json f = r; char nb[96]; std::snprintf(nb, sizeof nb, " The peak speed reached %.2f before this.", 0.37 + std::fmod(static_cast<double>(total) * 7.919, 40.0)); f["explanation"] = r.str_or("explanation", "") + nb; if (validate_report(*run, true, f).ok) { ++u4.n; u4.accepted += validate_report_strict(*run, cf, true, f, opt).ok; } }
      if (cf) {  // a story about a field the counterfactual did not change (note emptied, as a forger controls it)
        Json f = r; std::string other;
        std::vector<std::string> changed = scenario_diff(to_json(run->sc), to_json(cf->sc));
        for (const char* cand : {"gravity", "restitution", "friction", "damping", "drag", "kp", "fmax", "noise", "mass"}) {
          bool used = false; for (const std::string& pth : changed) used |= pth.find(cand) != std::string::npos;
          if (!used) { other = cand; break; }
        }
        f["explanation"] = "At the violating step the rule fails; this is caused by a wrong " + other + " setting in the integrator.";
        Json cfo = Json::object(); cfo["scenario_id"] = rep.at("counterfactual").str_or("scenario_id", ""); cfo["change"] = "see counterfactual"; f["counterfactual"] = cfo;
        if (validate_report(*run, true, f).ok) { ++u3.n; u3.accepted += validate_report_strict(*run, cf, true, f, opt).ok; }
      }
    }
  }
  std::printf("reports: %d valid in the run files, %d replayed\n", total, replayed);
  std::printf("honest reports accepted: default %d of %d, strict %d of %d\n", honest_default, replayed, honest_strict, replayed);
  std::printf("strict rejections of honest reports by check (a report can fail several): citations %d, prose numbers %d, cause not named %d\n", fail_cite, fail_prose, fail_cause);
  for (const auto& m : rejected_examples) std::printf("  e.g. %s\n", m.c_str());
  std::printf("forgeries the default validator accepts, then the strict mode:\n");
  std::printf("  U1 values off by 0.8%%:           %d of %d accepted by strict\n", u1.accepted, u1.n);
  std::printf("  U2 citations with no value:      %d of %d\n", u2.accepted, u2.n);
  std::printf("  U4 invented number in prose (0.4 to 40, 2 decimals): %d of %d\n", u4.accepted, u4.n);
  std::printf("  U3b cause names an unchanged field: %d of %d\n", u3.accepted, u3.n);
  return 0;
}
