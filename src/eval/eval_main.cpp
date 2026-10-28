// Evaluation harness. Reads the locked bug list and test seeds, runs a tester on each (bug, seed), and writes
// results JSON + a markdown table. Every number in the README comes from this program's output.
//   rc_eval baseline --lock data/eval_lock.json --out results
//   rc_eval agent --live --config configs/hosted.json --out results/agent_hosted [--total-budget-usd 6] [--run-budget-usd 0.25]
// The agent mode spends real money. It refuses to start a run that could push cumulative spend (including runs
// already on disk in --out) past --total-budget-usd, and it resumes: finished runs are never repeated.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "agent/agent.hpp"
#include "baseline/random_baseline.hpp"
#include "eval/ground_truth.hpp"
#include "shrink/shrink.hpp"
#include "viewer/view_export.hpp"

using namespace rc;

namespace {

std::string hex(uint64_t h) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(h));
  return b;
}

// True when the unmodified simulator (no seeded bug) also breaks the same rule on this scenario. Such a finding is a
// weakness of the base simulator, not evidence that a seeded bug was detected.
bool clean_sim_also_violates(const Scenario& sc, const std::string& rule) {
  Trace t = simulate(sc, 0);
  for (const Violation& v : check_rules(sc, t))
    if (v.rule == rule) return true;
  return false;
}

Json run_one(BugMask mask, uint64_t seed, const Budget& budget) {
  HuntResult h = random_baseline(mask, seed, budget);
  Json r = Json::object();
  r["seed"] = static_cast<double>(seed);
  r["found"] = h.found;
  r["scenarios_run"] = h.scenarios_run;
  r["steps_used"] = static_cast<double>(h.steps_used);
  r["wall_ms"] = h.wall_ms;
  if (h.found) {
    r["scenarios_until_first"] = h.scenarios_until_first;
    r["rule"] = h.first_violation.rule;
    r["attributable"] = mask == 0 ? false : !clean_sim_also_violates(h.first_failure, h.first_violation.rule);
    ShrinkResult s = shrink(h.first_failure, mask, h.first_violation.rule);
    r["shrink_ok"] = s.ok;
    r["size_before"] = s.size_before;
    r["size_after"] = s.size_after;
    r["bodies_after"] = static_cast<int>(s.scenario.bodies.size());
    r["steps_after"] = s.scenario.steps;
  }
  return r;
}

// ---------------------------------------------------------------------------------------------- agent mode

struct AgentEvalOpts {
  std::string config = "configs/hosted.json", out = "results/agent", reasoning_effort = "low", truth = "data/ground_truth.json";
  double total_budget = 6.0, run_budget = 0.25;
  int max_turns = 25;
  bool live = false;
  int agent_version = 1;  // 1 = frozen original agent, 2 = counterfactual-checked reports
  bool vision = true;     // v2 only: show rendered frames in the loop
  std::string only_bug;  // optional: restrict to one bug id (smoke / debugging)
};

std::string run_key(const std::string& bug, uint64_t seed) { return bug + "_" + std::to_string(seed); }

bool file_exists(const std::string& p) { return std::ifstream(p).good(); }

double median(std::vector<double> v) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

struct PlannedRun {
  std::string bug, split;  // bug "none" = control
  uint64_t seed;
};

std::vector<PlannedRun> plan_runs(const EvalLock& lock, const std::string& only_bug) {
  std::vector<PlannedRun> plan;
  for (uint64_t seed : lock.test_seeds)  // seeds outermost so a spend stop leaves balanced coverage
    for (const auto& lb : lock.bugs)
      if (only_bug.empty() || only_bug == lb.id) plan.push_back({lb.id, lb.split, seed});
  if (only_bug.empty())
    for (uint64_t seed : lock.control_seeds) plan.push_back({"none", "control", seed});
  return plan;
}

Json read_json_file(const std::string& p) { return Json::parse(read_file(p)); }

double spent_on_disk(const std::string& out, const std::vector<PlannedRun>& plan) {
  double total = 0;
  for (const auto& r : plan) {
    std::string f = out + "/runs/" + run_key(r.bug, r.seed) + ".json";
    if (file_exists(f)) total += read_json_file(f).num_or("cost_usd", 0);
  }
  return total;
}

Json execute_run(const PlannedRun& pr, const EvalLock& lock, const Json& truth, const AgentEvalOpts& o, const ApiConfig& api) {
  std::string dir = o.out + "/runs/" + run_key(pr.bug, pr.seed);
  if (std::system(("mkdir -p '" + dir + "'").c_str()) != 0) throw std::runtime_error("cannot create " + dir);
  BugMask mask = pr.bug == "none" ? 0 : bug_bit(domain_bug_index(lock.domain, pr.bug));

  MetaApiBackend live(api);
  RecordingBackend rec(live, dir + "/cache.jsonl");
  BudgetedModel model(rec, api.pricing, o.run_budget, dir + "/cost.jsonl");
  model.set_lifetime_ledger(default_ledger_path(), default_lifetime_cap_usd());
  ToolBudget tb;
  tb.max_steps = lock.max_steps;  // same simulation-step budget as the random baseline
  ToolSession tools(mask, tb, dir);
  tools.set_domain(lock.domain);
  AgentConfig ac;
  ac.domain = lock.domain;
  ac.model = api.model;
  ac.native_tools = api.native_tools;
  ac.max_turns = o.max_turns;
  ac.reasoning_effort = o.reasoning_effort;
  ac.version = o.agent_version;
  ac.vision_in_loop = o.vision;
  ac.out_dir = dir;
  std::ofstream events(dir + "/events.jsonl", std::ios::trunc);

  auto t0 = std::chrono::steady_clock::now();
  AgentResult res = run_agent(model, tools, ac, [&](const Json& ev) { events << ev.dump() << "\n"; });
  double wall_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  write_report_files(res, tools, dir);

  Json row = Json::object();
  row["bug"] = pr.bug;
  row["split"] = pr.split;
  row["seed"] = static_cast<double>(pr.seed);
  row["model"] = api.model;
  row["detected"] = res.found_failure;  // the engine confirmed a violation during the session
  row["report_submitted"] = res.report_submitted;
  row["report_valid"] = res.report_valid;  // shrunk + violated + citations match the recorded states
  row["rejected_reports"] = res.rejected_reports;
  row["stop_reason"] = res.stop_reason;
  row["turns"] = res.turns;
  row["scenarios_run"] = tools.usage().scenarios_run;
  row["scenarios_until_first"] = tools.usage().scenarios_until_first_violation;
  row["tool_calls"] = tools.usage().tool_calls;
  row["steps_used"] = static_cast<double>(tools.usage().steps_used);
  row["cost_usd"] = model.spent();
  row["api_calls"] = model.calls();
  row["api_latency_ms"] = model.total_latency_ms();
  row["wall_ms"] = wall_ms;
  row["agent_version"] = o.agent_version;
  row["vision_in_loop"] = o.agent_version >= 2 && o.vision;
  row["vision_done"] = res.vision_done;
  row["vision_agrees"] = res.vision_agrees;
  if (res.report_valid) {
    row["rule"] = res.rule;
    row["explanation"] = res.explanation;
    row["shrunk_size"] = scenario_size(scenario_from_json(res.minimal_scenario));
    if (o.agent_version >= 2 && res.counterfactual.is_object()) {
      row["counterfactual_paths"] = res.counterfactual.at("changed_paths");
      std::vector<std::string> ch;
      for (const auto& p : res.counterfactual.at("changed_paths").arr()) ch.push_back(p.str());
      if (pr.bug != "none") row["cause_localized"] = counterfactual_localizes(truth, pr.bug, ch);
    }
    if (pr.bug != "none") {
      const Json& gt = truth.at("bugs").at(pr.bug);
      bool rule_expected = false;
      for (const auto& r : gt.at("expected_rules").arr()) rule_expected |= r.str() == res.rule;
      row["rule_expected"] = rule_expected;
      row["explanation_names_cause"] = explanation_names_cause(truth, pr.bug, res.explanation);
    }
  }
  return row;
}

std::string pct(int k, int n) { return std::to_string(k) + "/" + std::to_string(n); }

void write_agent_summary(const AgentEvalOpts& o, const EvalLock& lock, const std::string& model_name) {
  Json baseline;
  bool have_baseline = file_exists("results/baseline.json");
  if (have_baseline) baseline = read_json_file("results/baseline.json");
  std::string md =
      "| bug | split | agent detected | agent valid report | valid and attributable | explanation names cause | random baseline found (attributable) | median scenarios to first violation | median shrunk size | cost USD (sum) |\n"
      "|---|---|---|---|---|---|---|---|---|---|\n";
  Json bugs = Json::array();
  double total_cost = 0, total_latency = 0;
  int total_runs = 0;
  for (const auto& lb : lock.bugs) {
    int n = 0, det = 0, valid = 0, expl = 0, attr = 0;
    std::vector<double> firsts, sizes;
    double cost = 0;
    for (uint64_t seed : lock.test_seeds) {
      std::string f = o.out + "/runs/" + run_key(lb.id, seed) + ".json";
      if (!file_exists(f)) continue;
      Json r = read_json_file(f);
      ++n; ++total_runs;
      cost += r.num_or("cost_usd", 0);
      total_latency += r.num_or("api_latency_ms", 0);
      if (r.bool_or("detected", false)) { ++det; firsts.push_back(r.num_or("scenarios_until_first", 0)); }
      if (r.bool_or("report_valid", false)) {
        ++valid;
        sizes.push_back(r.num_or("shrunk_size", 0));
        Json rep = read_json_file(o.out + "/runs/" + run_key(lb.id, seed) + "/report.json");
        if (!clean_sim_also_violates(scenario_from_json(rep.at("minimal_scenario")), rep.at("rule").str())) ++attr;
      }
      if (r.bool_or("explanation_names_cause", false)) ++expl;
    }
    total_cost += cost;
    std::string base = "n/a";
    if (have_baseline)
      for (const auto& b : baseline.at("bugs").arr())
        if (b.at("bug").str() == lb.id) base = pct(static_cast<int>(b.at("found_count").num()), static_cast<int>(b.at("seed_count").num())) + " (" +
                 std::to_string(static_cast<int>(b.num_or("attributable_count", -1))) + ")";
    char c[32];
    std::snprintf(c, sizeof c, "%.3f", cost);
    md += "| " + lb.id + " | " + lb.split + " | " + pct(det, n) + " | " + pct(valid, n) + " | " + pct(attr, n) + " | " + pct(expl, n) + " | " + base + " | " +
          (firsts.empty() ? "-" : std::to_string(static_cast<int>(median(firsts)))) + " | " +
          (sizes.empty() ? "-" : std::to_string(static_cast<int>(median(sizes)))) + " | " + c + " |\n";
    Json row = Json::object();
    row["bug"] = lb.id; row["split"] = lb.split; row["runs"] = n; row["detected"] = det; row["valid_reports"] = valid; row["valid_attributable"] = attr;
    row["explanation_names_cause"] = expl; row["cost_usd"] = cost;
    bugs.push(row);
  }
  int ctl_n = 0, ctl_detected = 0, ctl_valid = 0, ctl_submitted = 0;
  for (uint64_t seed : lock.control_seeds) {
    std::string f = o.out + "/runs/" + run_key("none", seed) + ".json";
    if (!file_exists(f)) continue;
    Json r = read_json_file(f);
    ++ctl_n; ++total_runs;
    total_cost += r.num_or("cost_usd", 0);
    total_latency += r.num_or("api_latency_ms", 0);
    ctl_detected += r.bool_or("detected", false);
    ctl_valid += r.bool_or("report_valid", false);
    ctl_submitted += r.bool_or("report_submitted", false);
  }
  char t[400];
  std::snprintf(t, sizeof t, "\nControl runs (no bug enabled): %d runs, %d engine-confirmed violations, %d reports submitted, %d reports accepted.\n"
                "Total: %d runs, $%.3f API cost, %.1f s of API latency (model %s).\n", ctl_n, ctl_detected, ctl_submitted, ctl_valid,
                total_runs, total_cost, total_latency / 1000.0, model_name.c_str());
  md += t;
  Json out = Json::object();
  out["tester"] = "agent";
  out["model"] = model_name;
  out["lock_hash"] = hex(lock.hash);
  out["bugs"] = bugs;
  out["control_runs"] = ctl_n;
  out["control_detected"] = ctl_detected;
  out["control_reports_accepted"] = ctl_valid;
  out["total_cost_usd"] = total_cost;
  std::ofstream(o.out + "/summary.json") << out.dump() << "\n";
  std::ofstream(o.out + "/summary.md") << "<!-- generated by rc_eval agent; lock hash " << hex(lock.hash) << " -->\n" << md;
  std::cout << md;
}

int agent_eval(const EvalLock& lock, const AgentEvalOpts& o) {
  if (!o.live) { std::cerr << "agent mode calls the real API; pass --live to confirm\n"; return 2; }
  ApiConfig api = ApiConfig::from_json(Json::parse(read_file(o.config)));
  Json truth = Json::parse(read_file(o.truth));
  if (std::system(("mkdir -p '" + o.out + "/runs'").c_str()) != 0) return 1;
  std::vector<PlannedRun> plan = plan_runs(lock, o.only_bug);
  double spent = spent_on_disk(o.out, plan);
  std::cout << "lifetime ledger: $" << ledger_total_usd(default_ledger_path()) << " of $" << default_lifetime_cap_usd() << "\n";
  std::cout << "planned runs: " << plan.size() << ", already spent on disk: $" << spent << ", total cap $" << o.total_budget
            << ", per-run cap $" << o.run_budget << "\n";
  int done = 0, skipped_done = 0;
  bool capped = false;
  for (const auto& pr : plan) {
    std::string f = o.out + "/runs/" + run_key(pr.bug, pr.seed) + ".json";
    if (file_exists(f)) { ++skipped_done; continue; }
    if (spent + o.run_budget > o.total_budget) { capped = true; break; }
    Json row;
    try {
      row = execute_run(pr, lock, truth, o, api);
    } catch (const std::exception& e) {
      std::cerr << "run " << run_key(pr.bug, pr.seed) << " failed: " << e.what() << "\n";
      continue;  // not recorded, so a later invocation retries it
    }
    spent += row.num_or("cost_usd", 0);
    std::ofstream(f) << row.dump() << "\n";
    ++done;
    std::cout << run_key(pr.bug, pr.seed) << ": detected=" << row.bool_or("detected", false) << " valid=" << row.bool_or("report_valid", false)
              << " stop=" << row.str_or("stop_reason", "") << " cost=$" << row.num_or("cost_usd", 0) << " cumulative=$" << spent << "\n";
  }
  if (capped) std::cout << "STOPPED: the next run could exceed the total spend cap ($" << o.total_budget << "); finished runs are saved\n";
  std::cout << "runs this invocation: " << done << ", already done: " << skipped_done << "\n";
  write_agent_summary(o, lock, api.model);
  return capped ? 3 : 0;
}


// ---------------------------------------------------------------------------------------------- arm comparison

std::string wilson(int k, int n) {  // "k/n [lo-hi]" with the 95% Wilson interval
  if (n == 0) return "-";
  const double z = 1.96, p = static_cast<double>(k) / n, d = 1 + z * z / n;
  const double c = (p + z * z / (2 * n)) / d, h = z * std::sqrt(p * (1 - p) / n + z * z / (4.0 * n * n)) / d;
  char b[64];
  std::snprintf(b, sizeof b, "%d/%d [%.2f-%.2f]", k, n, std::fmax(0, c - h), std::fmin(1, c + h));
  return b;
}

struct ArmStats {
  int n = 0, detected = 0, valid = 0, attributable = 0, expl_kw = 0, with_cf = 0, localized = 0;
  double cost = 0;
  std::vector<double> firsts;
};

int compare_arms(const EvalLock& lock, const std::string& arms_arg, const std::string& baseline_path, const std::string& out_path) {
  std::vector<std::pair<std::string, std::string>> arms;  // name -> directory
  std::stringstream ss(arms_arg);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    size_t eq = tok.find('=');
    if (eq == std::string::npos) { std::cerr << "--arms expects name=dir,name=dir\n"; return 2; }
    arms.push_back({tok.substr(0, eq), tok.substr(eq + 1)});
  }
  Json baseline;
  bool have_baseline = !baseline_path.empty() && file_exists(baseline_path);
  if (have_baseline) baseline = read_json_file(baseline_path);
  std::string md = "<!-- generated by rc_eval compare; lock hash " + hex(lock.hash) + " -->\n"
                   "Counts are k/n with a 95% Wilson interval. 'explained (keywords)' is the keyword scorer (see docs/SCORER_AUDIT.md: "
                   "precision about 0.6); 'cause localized' is the counterfactual check (v2 arms only).\n\n"
                   "| bug | arm | detected | valid and attributable | explained (keywords) | cause localized | median scenarios to first | mean cost USD |\n"
                   "|---|---|---|---|---|---|---|---|\n";
  Json jout = Json::array();
  std::map<std::string, ArmStats> pooled;
  Json truth = Json::parse(read_file("data/ground_truth.json"));
  (void)truth;
  for (const auto& lb : lock.bugs) {
    if (have_baseline)
      for (const auto& b : baseline.at("bugs").arr())
        if (b.at("bug").str() == lb.id)
          md += "| " + lb.id + " | random baseline | " + wilson(static_cast<int>(b.num_or("found_count", 0)), static_cast<int>(b.num_or("seed_count", 0))) +
                " | " + wilson(static_cast<int>(b.num_or("attributable_count", 0)), static_cast<int>(b.num_or("seed_count", 0))) + " | - | - | - | $0 |\n";
    for (const auto& arm : arms) {
      ArmStats st;
      for (uint64_t seed : lock.test_seeds) {
        std::string f = arm.second + "/runs/" + run_key(lb.id, seed) + ".json";
        if (!file_exists(f)) continue;
        Json r = read_json_file(f);
        ++st.n;
        st.cost += r.num_or("cost_usd", 0);
        if (r.bool_or("detected", false)) { ++st.detected; st.firsts.push_back(r.num_or("scenarios_until_first", 0)); }
        if (r.bool_or("report_valid", false)) {
          ++st.valid;
          Json rep = read_json_file(arm.second + "/runs/" + run_key(lb.id, seed) + "/report.json");
          if (!clean_sim_also_violates(scenario_from_json(rep.at("minimal_scenario"), lock.domain), rep.at("rule").str())) ++st.attributable;
        }
        if (r.bool_or("explanation_names_cause", false)) ++st.expl_kw;
        if (r.has("cause_localized")) { ++st.with_cf; st.localized += r.bool_or("cause_localized", false); }
      }
      ArmStats& P = pooled[arm.first];
      P.n += st.n; P.detected += st.detected; P.valid += st.valid; P.attributable += st.attributable; P.expl_kw += st.expl_kw;
      P.with_cf += st.with_cf; P.localized += st.localized; P.cost += st.cost;
      P.firsts.insert(P.firsts.end(), st.firsts.begin(), st.firsts.end());
      char c[32];
      std::snprintf(c, sizeof c, "%.3f", st.n ? st.cost / st.n : 0.0);
      md += "| " + lb.id + " | " + arm.first + " | " + wilson(st.detected, st.n) + " | " + wilson(st.attributable, st.n) + " | " + wilson(st.expl_kw, st.n) +
            " | " + (st.with_cf ? wilson(st.localized, st.n) : std::string("n/a")) + " | " + (st.firsts.empty() ? "-" : std::to_string(static_cast<int>(median(st.firsts)))) + " | " + c + " |\n";
      Json j = Json::object();
      j["bug"] = lb.id; j["arm"] = arm.first; j["n"] = st.n; j["detected"] = st.detected; j["attributable"] = st.attributable;
      j["explained_keywords"] = st.expl_kw; j["localized"] = st.localized; j["cost_usd"] = st.cost;
      jout.push(j);
    }
  }
  md += "| **all** | | | | | | | |\n";
  for (const auto& arm : arms) {
    const ArmStats& P = pooled[arm.first];
    char c[32];
    std::snprintf(c, sizeof c, "%.3f", P.n ? P.cost / P.n : 0.0);
    md += "| all | " + arm.first + " | " + wilson(P.detected, P.n) + " | " + wilson(P.attributable, P.n) + " | " + wilson(P.expl_kw, P.n) + " | " +
          (P.with_cf ? wilson(P.localized, P.n) : std::string("n/a")) + " | " + (P.firsts.empty() ? "-" : std::to_string(static_cast<int>(median(P.firsts)))) + " | " + c + " |\n";
  }
  // controls: any report on a clean simulator is a false report
  md += "\n| arm | control runs | engine-confirmed violations | reports accepted |\n|---|---|---|---|\n";
  for (const auto& arm : arms) {
    int n = 0, det = 0, acc = 0;
    for (uint64_t seed : lock.control_seeds) {
      std::string f = arm.second + "/runs/" + run_key("none", seed) + ".json";
      if (!file_exists(f)) continue;
      Json r = read_json_file(f);
      ++n; det += r.bool_or("detected", false); acc += r.bool_or("report_valid", false);
    }
    md += "| " + arm.first + " | " + std::to_string(n) + " | " + std::to_string(det) + " | " + std::to_string(acc) + " |\n";
  }
  std::cout << md;
  if (!out_path.empty()) {
    std::ofstream(out_path) << md;
    std::ofstream(out_path + ".json") << jout.dump() << "\n";
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string mode = argc > 1 ? argv[1] : "";
  std::string lock_path = "data/eval_lock.json", out_dir;
  bool append = false;
  std::string arms_arg, baseline_arg;
  AgentEvalOpts ao;
  for (int i = 2; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--live") { ao.live = true; continue; }
    if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; return 2; }
    std::string v = argv[++i];
    if (a == "--lock") lock_path = v;
    else if (a == "--out") out_dir = v;
    else if (a == "--config") ao.config = v;
    else if (a == "--total-budget-usd") ao.total_budget = std::atof(v.c_str());
    else if (a == "--run-budget-usd") ao.run_budget = std::atof(v.c_str());
    else if (a == "--max-turns") ao.max_turns = std::atoi(v.c_str());
    else if (a == "--reasoning-effort") ao.reasoning_effort = v;
    else if (a == "--only-bug") ao.only_bug = v;
    else if (a == "--arms") arms_arg = v;
    else if (a == "--baseline") baseline_arg = v;
    else if (a == "--agent-version") ao.agent_version = std::atoi(v.c_str());
    else if (a == "--vision") ao.vision = v != "0";
    else if (a == "--append") append = v == "1";  // export: keep index rows from earlier exports
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  if (mode == "export") {  // --append 1 merges into an existing index.json (several locks/domains in one viewer)  // viewer documents for every saved run under --out/runs, written to --viewer-dir
    std::string dir = out_dir.empty() ? "results/agent_hosted" : out_dir;
    EvalLock lock = load_lock(lock_path);
    std::string vdir = ao.config == "configs/hosted.json" ? "web/data" : ao.config;  // --config reused as the target directory
    if (std::system(("mkdir -p '" + vdir + "'").c_str()) != 0) return 1;
    Json index = Json::array();
    if (append && file_exists(vdir + "/index.json")) index = read_json_file(vdir + "/index.json");
    std::vector<PlannedRun> plan = plan_runs(lock, "");
    for (const auto& pr : plan) {
      std::string key = run_key(pr.bug, pr.seed), rd = dir + "/runs/" + key;
      if (!file_exists(rd + "/events.jsonl")) continue;
      BugMask m = pr.bug == "none" ? 0 : bug_bit(domain_bug_index(lock.domain, pr.bug));
      Json doc = build_run_view(rd, m, lock.domain);
      doc["id"] = key;
      std::ofstream(vdir + "/" + key + ".json") << doc.dump() << "\n";
      Json row = Json::object();
      row["domain"] = lock.domain;
      row["id"] = key; row["bug"] = pr.bug; row["split"] = pr.split; row["seed"] = static_cast<double>(pr.seed);
      row["status"] = doc.str_or("status", "");
      row["usd"] = doc.at("cost").num_or("usd", 0);
      Json kept = Json::array();
      for (const auto& old : index.arr()) if (old.str_or("id", "") != key) kept.push(old);
      kept.push(row);
      index = kept;
    }
    std::ofstream(vdir + "/index.json") << index.dump() << "\n";
    if (lock.domain == "rigid2d" && file_exists(dir + "/summary.json") && file_exists("results/baseline.json")) {  // scoreboard: agent vs random baseline
      Json sum = read_json_file(dir + "/summary.json"), base = read_json_file("results/baseline.json");
      Json rows = Json::array();
      for (const auto& b : sum.at("bugs").arr()) {
        Json r = Json::object();
        r["bug"] = b.at("bug").str(); r["split"] = b.at("split").str();
        r["runs"] = b.num_or("runs", 0); r["agent_detected"] = b.num_or("detected", 0); r["agent_valid"] = b.num_or("valid_reports", 0);
        r["agent_valid_attributable"] = b.num_or("valid_attributable", 0); r["agent_explained"] = b.num_or("explanation_names_cause", 0);
        r["agent_cost_usd"] = b.num_or("cost_usd", 0);
        for (const auto& x : base.at("bugs").arr())
          if (x.at("bug").str() == b.at("bug").str()) {
            r["baseline_found"] = x.num_or("found_count", 0); r["baseline_attributable"] = x.num_or("attributable_count", 0);
          }
        rows.push(r);
      }
      Json sb = Json::object();
      sb["title"] = "Agent v1 vs random baseline (simulator v1, locked set v1)";
      sb["model"] = sum.str_or("model", "");
      sb["rows"] = rows;
      sb["control_runs"] = sum.num_or("control_runs", 0);
      sb["control_detected"] = sum.num_or("control_detected", 0);
      sb["total_cost_usd"] = sum.num_or("total_cost_usd", 0);
      std::ofstream(vdir + "/scoreboard.json") << sb.dump() << "\n";
    }
    std::cout << "exported " << index.arr().size() << " runs to " << vdir << "\n";
    return 0;
  }
  if (mode == "spend") {  // print the lifetime ledger total against the cap
    std::printf("lifetime spend $%.4f of cap $%.2f (%s)\n", ledger_total_usd(default_ledger_path()), default_lifetime_cap_usd(), default_ledger_path().c_str());
    return 0;
  }
  if (mode == "summarize") {  // regenerate the agent summary from saved runs; no API calls
    ao.out = out_dir.empty() ? "results/agent" : out_dir;
    EvalLock lock = load_lock(lock_path);
    std::string model = read_json_file(ao.out + "/runs/" + lock.bugs.at(0).id + "_" + std::to_string(lock.test_seeds.at(0)) + ".json").str_or("model", "unknown");
    write_agent_summary(ao, lock, model);
    return 0;
  }
  if (mode == "compare") return compare_arms(load_lock(lock_path), arms_arg, baseline_arg, out_dir);
  if (mode == "agent") {
    ao.out = out_dir.empty() ? "results/agent" : out_dir;
    return agent_eval(load_lock(lock_path), ao);
  }
  if (out_dir.empty()) out_dir = "results";
  if (mode != "baseline") {
    std::cerr << "usage: rc_eval baseline [--lock FILE] [--out DIR]\n"
                 "       rc_eval agent --live [--config FILE] [--out DIR] [--total-budget-usd N] [--run-budget-usd N] [--only-bug bNN]\n";
    return 2;
  }
  EvalLock lock = load_lock(lock_path);
  Budget budget;
  budget.max_steps = lock.max_steps;
  budget.max_wall_ms = lock.max_wall_ms;
  budget.domain = lock.domain;

  Json out = Json::object();
  out["tester"] = "random_baseline";
  out["lock_hash"] = hex(lock.hash);
  out["budget_steps"] = static_cast<double>(lock.max_steps);
  Json bugs = Json::array();
  std::string md = "| bug | split | found | found and attributable to the seeded bug | median scenarios to first violation | rules |\n|---|---|---|---|---|---|\n";
  for (const auto& lb : lock.bugs) {
    int idx = domain_bug_index(lock.domain, lb.id);
    if (idx < 0) { std::cerr << "unknown bug in lock: " << lb.id << "\n"; return 2; }
    Json row = Json::object();
    row["bug"] = lb.id;
    row["split"] = lb.split;
    Json runs = Json::array();
    int found = 0, attributable = 0;
    std::vector<int> firsts;
    std::string rules;
    for (uint64_t seed : lock.test_seeds) {
      Json r = run_one(bug_bit(idx), seed, budget);
      if (r.at("found").boolean()) {
        ++found;
        if (r.at("attributable").boolean()) ++attributable;
        firsts.push_back(static_cast<int>(r.at("scenarios_until_first").num()));
        if (rules.find(r.at("rule").str()) == std::string::npos) rules += (rules.empty() ? "" : ", ") + r.at("rule").str();
      }
      runs.push(r);
    }
    row["runs"] = runs;
    row["found_count"] = found;
    row["attributable_count"] = attributable;
    row["seed_count"] = static_cast<int>(lock.test_seeds.size());
    std::sort(firsts.begin(), firsts.end());
    char med[16] = "-";
    if (!firsts.empty()) std::snprintf(med, sizeof med, "%d", firsts[firsts.size() / 2]);
    bugs.push(row);
    md += "| " + lb.id + " | " + lb.split + " | " + std::to_string(found) + "/" + std::to_string(lock.test_seeds.size()) +
          " | " + std::to_string(attributable) + "/" + std::to_string(lock.test_seeds.size()) + " | " + med + " | " + rules + " |\n";
  }
  out["bugs"] = bugs;
  // controls: the unmodified simulation must produce no violations
  Json controls = Json::array();
  int false_pos = 0;
  for (uint64_t seed : lock.control_seeds) {
    Json r = run_one(0, seed, budget);
    if (r.at("found").boolean()) ++false_pos;
    controls.push(r);
  }
  out["controls"] = controls;
  out["control_false_positives"] = false_pos;
  md += "\nControl runs (no bug enabled): " + std::to_string(false_pos) + " false positives in " +
        std::to_string(lock.control_seeds.size()) + " runs.\n";

  std::string cmd = "mkdir -p '" + out_dir + "'";
  if (std::system(cmd.c_str()) != 0) { std::cerr << "cannot create " << out_dir << "\n"; return 1; }
  std::ofstream(out_dir + "/baseline.json") << out.dump() << "\n";
  std::ofstream(out_dir + "/baseline.md") << "<!-- generated by rc_eval baseline; lock hash " << hex(lock.hash) << " -->\n" << md;
  std::cout << md;
  return 0;
}
