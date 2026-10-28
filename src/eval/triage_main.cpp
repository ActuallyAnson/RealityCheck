// rc_triage: "fuzzer finds, model explains". For each (bug, seed) a random-sampling fuzzer finds a failing scenario (the
// model plays no part), then two things try to localize the cause on it:
//   (a) agent v2 in triage mode: shrink, inspect, one-change counterfactual, grounded report (needs --live, costs money);
//   (b) a no-model localizer: shrink, then try single-field edits and take the smallest edit that removes the failure.
// Both are scored the same way, by whether the changed field is a hidden trigger field of the seeded bug (data/ground_truth.json),
// which is harness-only knowledge. No keyword matching.
//   rc_triage run --lock data/eval_lock_triage.json --out results/triage [--live] [--no-model-only] [--total-budget-usd 6]
//   rc_triage summarize --lock data/eval_lock_triage.json --out results/triage
#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

#include "agent/agent.hpp"
#include "baseline/random_baseline.hpp"
#include "eval/ground_truth.hpp"
#include "model/model.hpp"
#include "shrink/shrink.hpp"
#include "sim/domains.hpp"

using namespace rc;

namespace {
void leaves(Json& j, std::vector<Json*>& out, const std::string& key) {
  if (j.is_object()) { std::vector<std::string> ks; for (auto& kv : j.obj()) ks.push_back(kv.first); for (auto& k : ks) leaves(j[k], out, k); }
  else if (j.is_array()) { for (Json& e : j.arr()) leaves(e, out, key); }
  else if ((j.is_number() || j.is_bool()) && key != "seed" && key != "steps") out.push_back(&j);
}

bool still_violates(const Scenario& sc, BugMask mask, const std::string& rule) {
  for (const Violation& v : check_rules(sc, simulate(sc, mask))) if (v.rule == rule) return true;
  return false;
}

struct LocalizerResult {
  bool found = false;
  std::vector<std::string> paths;
  int removing = 0, removing_localized = 0;
};

// No-model baseline: among single-field edits of the shrunk scenario that make the rule pass (same steps), the smallest relative edit.
LocalizerResult localize_no_model(const Scenario& shrunk, BugMask mask, const std::string& rule, const Json& truth, const std::string& bug) {
  LocalizerResult out;
  Json base = to_json(shrunk);
  Json probe = base;
  std::vector<Json*> ls;
  leaves(probe, ls, "");
  double best = 1e300;
  for (size_t li = 0; li < ls.size(); ++li) {
    std::vector<Json> alts;
    if (ls[li]->is_bool()) alts.push_back(!ls[li]->boolean());
    else {
      double x = ls[li]->num();
      bool integral = x == std::floor(x);
      for (double f : {0.9, 1.1, 0.8, 1.25, 0.5, 2.0, 0.0}) {
        double nx = x * f;
        if (integral) nx = std::round(nx);
        if (nx != x) alts.push_back(nx);
      }
    }
    for (const Json& alt : alts) {
      Json cand = base;
      std::vector<Json*> cl;
      leaves(cand, cl, "");
      *cl[li] = alt;
      try {
        Scenario sc = scenario_from_json(cand, shrunk.domain);
        if (!validate(sc).empty() || still_violates(sc, mask, rule)) continue;
        std::vector<std::string> paths = scenario_diff(base, to_json(sc));
        if (paths.empty()) continue;
        ++out.removing;
        if (counterfactual_localizes(truth, bug, paths)) ++out.removing_localized;
        double rel = ls[li]->is_bool() ? 1.0 : std::fabs(alt.num() - ls[li]->num()) / std::max(std::fabs(ls[li]->num()), 1e-9);
        if (rel < best) { best = rel; out.found = true; out.paths = paths; }
      } catch (const std::exception&) {}
    }
  }
  return out;
}

std::string wilson(int k, int n) {
  if (n == 0) return "-";
  double p = static_cast<double>(k) / n, z = 1.96, d = 1 + z * z / n, c = p + z * z / (2 * n), h = z * std::sqrt(p * (1 - p) / n + z * z / (4.0 * n * n));
  char b[64];
  std::snprintf(b, sizeof b, "%d/%d [%.2f-%.2f]", k, n, std::max(0.0, (c - h) / d), std::min(1.0, (c + h) / d));
  return b;
}
bool exists(const std::string& p) { return std::ifstream(p).good(); }
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::cerr << "usage: rc_triage run|summarize --lock FILE --out DIR [--live] [--no-model-only] [--total-budget-usd N]\n"; return 2; }
  std::string mode = argv[1], lock_path = "data/eval_lock_triage.json", out = "results/triage", config = "configs/hosted.json";
  bool live = false, no_model_only = false;
  double total_budget = 6.0, run_budget = 0.25;
  for (int i = 2; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--lock") lock_path = val(); else if (a == "--out") out = val(); else if (a == "--live") live = true;
    else if (a == "--no-model-only") no_model_only = true; else if (a == "--total-budget-usd") total_budget = std::atof(val().c_str());
    else if (a == "--run-budget-usd") run_budget = std::atof(val().c_str()); else if (a == "--config") config = val();
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  Json lock = Json::parse(read_file(lock_path));
  Json truth = Json::parse(read_file("data/ground_truth.json"));
  std::vector<std::pair<std::string, std::string>> plan;  // (domain, bug)
  for (auto& kv : lock.at("domains").obj()) for (const Json& b : kv.second.arr()) plan.push_back({kv.first, b.str()});
  std::vector<uint64_t> seeds;
  for (const Json& s : lock.at("seeds").arr()) seeds.push_back(static_cast<uint64_t>(s.num()));
  const long long max_steps = static_cast<long long>(lock.at("fuzzer_budget_steps").num());
  const int max_turns = static_cast<int>(lock.at("agent").num_or("max_turns", 25));
  std::string effort = lock.at("agent").str_or("reasoning_effort", "low");

  if (mode == "run") {
    if (std::system(("mkdir -p '" + out + "/runs'").c_str()) != 0) return 1;
    ApiConfig api = ApiConfig::from_json(Json::parse(read_file(config)));
    double spent = 0;
    for (auto& pb : plan) for (uint64_t sd : seeds) {
      std::string f = out + "/runs/" + pb.second + "_" + std::to_string(sd) + ".json";
      if (exists(f)) spent += Json::parse(read_file(f)).num_or("cost_usd", 0);
    }
    for (uint64_t sd : seeds) for (auto& pb : plan) {
      const std::string& domain = pb.first;
      const std::string& bug = pb.second;
      std::string key = bug + "_" + std::to_string(sd), f = out + "/runs/" + key + ".json", dir = out + "/runs/" + key;
      if (exists(f)) continue;
      BugMask mask = bug_bit(domain_bug_index(domain, bug));
      Json row = Json::object();
      row["bug"] = bug; row["domain"] = domain; row["seed"] = static_cast<double>(sd);
      Budget bud; bud.max_steps = max_steps; bud.max_wall_ms = 1e12; bud.domain = domain;
      HuntResult h = random_baseline(mask, 9000 + sd, bud);
      row["fuzz_scenarios"] = h.scenarios_run;
      std::string rule = h.found ? h.first_violation.rule : "";
      if (!h.found) row["status"] = "fuzzer_found_nothing";
      else if (still_violates(h.first_failure, 0, rule)) row["status"] = "fuzzer_failure_not_attributable";
      else row["status"] = "triaged";
      row["rule"] = rule;
      if (row.at("status").str() == "triaged") {
        ShrinkResult sr = shrink(h.first_failure, mask, rule);
        LocalizerResult lr = localize_no_model(sr.scenario, mask, rule, truth, bug);
        Json bp = Json::array();
        for (auto& p : lr.paths) bp.push(p);
        row["baseline_found"] = lr.found; row["baseline_paths"] = bp;
        row["baseline_localized"] = lr.found && counterfactual_localizes(truth, bug, lr.paths);
        row["baseline_removing_edits"] = lr.removing; row["baseline_removing_localized"] = lr.removing_localized;
        row["shrunk_size"] = scenario_size(sr.scenario);
        if (live && !no_model_only) {
          if (spent + run_budget > total_budget) { std::cerr << "total budget reached; stopping\n"; return 0; }
          if (std::system(("mkdir -p '" + dir + "'").c_str()) != 0) return 1;
          MetaApiBackend lb(api);
          RecordingBackend rec(lb, dir + "/cache.jsonl");
          BudgetedModel model(rec, api.pricing, run_budget, dir + "/cost.jsonl");
          model.set_lifetime_ledger(default_ledger_path(), default_lifetime_cap_usd());
          ToolBudget tb; tb.max_steps = max_steps;
          ToolSession tools(mask, tb, dir);
          tools.set_domain(domain);
          AgentConfig ac;
          ac.domain = domain; ac.model = api.model; ac.native_tools = api.native_tools; ac.max_turns = max_turns; ac.reasoning_effort = effort;
          ac.version = 2; ac.vision_in_loop = lock.at("agent").bool_or("vision", true); ac.out_dir = dir; ac.triage = to_json(h.first_failure);
          std::ofstream events(dir + "/events.jsonl", std::ios::trunc);
          AgentResult res = run_agent(model, tools, ac, [&](const Json& ev) { events << ev.dump() << "\n"; });
          write_report_files(res, tools, dir);
          row["agent_report_valid"] = res.report_valid; row["agent_stop_reason"] = res.stop_reason; row["agent_turns"] = res.turns;
          row["agent_rejected_reports"] = res.rejected_reports; row["cost_usd"] = model.spent(); spent += model.spent();
          if (res.report_valid) {
            row["agent_explanation"] = res.explanation;
            if (res.counterfactual.is_object()) {
              row["agent_paths"] = res.counterfactual.at("changed_paths");
              std::vector<std::string> ch;
              for (const auto& p : res.counterfactual.at("changed_paths").arr()) ch.push_back(p.str());
              row["agent_localized"] = counterfactual_localizes(truth, bug, ch);
            }
            row["agent_explanation_names_cause"] = explanation_names_cause(truth, bug, res.explanation);
          }
        }
      }
      std::ofstream(f) << row.dump() << "\n";
      std::cout << key << " " << row.at("status").str() << (row.has("agent_localized") ? (row.bool_or("agent_localized", false) ? " agent:localized" : " agent:missed") : "")
                << (row.has("baseline_localized") ? (row.bool_or("baseline_localized", false) ? " baseline:localized" : " baseline:missed") : "") << "\n";
      std::cout.flush();
    }
    return 0;
  }
  if (mode == "summarize") {
    std::string md = "| domain | bug | fuzzer found it | agent valid report | agent cause localized | no-model localizer cause localized | share of single-field removing edits that hit the cause |\n|---|---|---|---|---|---|---|\n";
    int T = 0, A = 0, AL = 0, BL = 0, N = 0;
    double cost = 0;
    int rem = 0, remloc = 0, a_only = 0, b_only = 0, s_n = 0, s_a = 0, s_b = 0, s_a_only = 0, s_b_only = 0;
    for (auto& pb : plan) {
      int n = 0, tri = 0, av = 0, al = 0, bl = 0, r = 0, rl = 0;
      for (uint64_t sd : seeds) {
        std::string f = out + "/runs/" + pb.second + "_" + std::to_string(sd) + ".json";
        if (!exists(f)) continue;
        Json row = Json::parse(read_file(f));
        ++n; cost += row.num_or("cost_usd", 0);
        if (row.str_or("status", "") != "triaged") continue;
        ++tri; bl += row.bool_or("baseline_localized", false);
        r += static_cast<int>(row.num_or("baseline_removing_edits", 0)); rl += static_cast<int>(row.num_or("baseline_removing_localized", 0));
        if (row.has("agent_report_valid")) { av += row.bool_or("agent_report_valid", false); al += row.bool_or("agent_localized", false); }
        if (row.has("agent_report_valid")) {
          bool a = row.bool_or("agent_localized", false), b = row.bool_or("baseline_localized", false);
          a_only += a && !b; b_only += b && !a;
          if (row.has("agent_paths") && row.at("agent_paths").arr().size() == 1) { ++s_n; s_a += a; s_b += b; s_a_only += a && !b; s_b_only += b && !a; }
        }
      }
      char sh[32];
      std::snprintf(sh, sizeof sh, "%.0f%%", r ? 100.0 * rl / r : 0.0);
      md += "| " + pb.first + " | " + pb.second + " | " + std::to_string(tri) + "/" + std::to_string(n) + " | " + std::to_string(av) + "/" + std::to_string(tri) + " | " + std::to_string(al) + "/" + std::to_string(tri) +
            " | " + std::to_string(bl) + "/" + std::to_string(tri) + " | " + (r ? sh : "-") + " |\n";
      T += tri; N += n; A += av; AL += al; BL += bl; rem += r; remloc += rl;
    }
    md += "\nAll bugs with a fuzzer failure: agent valid report " + wilson(A, T) + ", agent cause localized " + wilson(AL, T) + ", no-model localizer cause localized " + wilson(BL, T) +
          ". Runs with a fuzzer failure: " + std::to_string(T) + " of " + std::to_string(N) + ". Agent cost $" + std::to_string(cost).substr(0, 6) + ".\n";
    auto sign_p = [](int x, int y) {  // exact two-sided sign test on the discordant pairs
      int n = x + y, k = std::min(x, y);
      if (n == 0) return 1.0;
      double tail = 0, c = 1;  // c = C(n, j)
      for (int j = 0; j <= k; ++j) { tail += c; c = c * (n - j) / (j + 1); }
      return std::min(1.0, 2 * tail / std::pow(2.0, n));
    };
    char ps[400];
    std::snprintf(ps, sizeof ps, "\nPaired comparison (same failures): agent localized and baseline did not %d times, baseline and not agent %d times; exact sign test p = %.4f. "
                  "Restricted to runs where the agent's counterfactual changed exactly one field (as the baseline's does): %d runs, agent %d, baseline %d, agent-only %d, baseline-only %d, p = %.4f.\n",
                  a_only, b_only, sign_p(a_only, b_only), s_n, s_a, s_b, s_a_only, s_b_only, sign_p(s_a_only, s_b_only));
    md += ps;
    std::ofstream(out + "/summary.md") << "<!-- generated by rc_triage summarize -->\n" << md;
    std::cout << md;
    return 0;
  }
  std::cerr << "mode must be run or summarize\n";
  return 2;
}
