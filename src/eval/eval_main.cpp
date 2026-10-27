// Evaluation harness. Reads the locked bug list and test seeds, runs a tester on each (bug, seed), and writes
// results JSON + a markdown table. Every number in the README comes from this program's output.
//   rc_eval baseline --lock data/eval_lock.json --out results
//   rc_eval agent --live --config configs/hosted.json --out results/agent_hosted [--total-budget-usd 6] [--run-budget-usd 0.25]
// The agent mode spends real money. It refuses to start a run that could push cumulative spend (including runs
// already on disk in --out) past --total-budget-usd, and it resumes: finished runs are never repeated.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "agent/agent.hpp"
#include "baseline/random_baseline.hpp"
#include "eval/ground_truth.hpp"
#include "shrink/shrink.hpp"

using namespace rc;

namespace {

std::string hex(uint64_t h) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(h));
  return b;
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
  BugMask mask = pr.bug == "none" ? 0 : bug_bit(bug_index(pr.bug));

  MetaApiBackend live(api);
  RecordingBackend rec(live, dir + "/cache.jsonl");
  BudgetedModel model(rec, api.pricing, o.run_budget, dir + "/cost.jsonl");
  ToolBudget tb;
  tb.max_steps = lock.max_steps;  // same simulation-step budget as the random baseline
  ToolSession tools(mask, tb, dir);
  AgentConfig ac;
  ac.model = api.model;
  ac.native_tools = api.native_tools;
  ac.max_turns = o.max_turns;
  ac.reasoning_effort = o.reasoning_effort;
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
  row["vision_done"] = res.vision_done;
  row["vision_agrees"] = res.vision_agrees;
  if (res.report_valid) {
    row["rule"] = res.rule;
    row["explanation"] = res.explanation;
    row["shrunk_size"] = scenario_size(scenario_from_json(res.minimal_scenario));
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
      "| bug | split | agent detected | agent valid report | explanation names cause | random baseline found | median scenarios to first violation | median shrunk size | cost USD (sum) |\n"
      "|---|---|---|---|---|---|---|---|---|\n";
  Json bugs = Json::array();
  double total_cost = 0, total_latency = 0;
  int total_runs = 0;
  for (const auto& lb : lock.bugs) {
    int n = 0, det = 0, valid = 0, expl = 0;
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
      if (r.bool_or("report_valid", false)) { ++valid; sizes.push_back(r.num_or("shrunk_size", 0)); }
      if (r.bool_or("explanation_names_cause", false)) ++expl;
    }
    total_cost += cost;
    std::string base = "n/a";
    if (have_baseline)
      for (const auto& b : baseline.at("bugs").arr())
        if (b.at("bug").str() == lb.id) base = pct(static_cast<int>(b.at("found_count").num()), static_cast<int>(b.at("seed_count").num()));
    char c[32];
    std::snprintf(c, sizeof c, "%.3f", cost);
    md += "| " + lb.id + " | " + lb.split + " | " + pct(det, n) + " | " + pct(valid, n) + " | " + pct(expl, n) + " | " + base + " | " +
          (firsts.empty() ? "-" : std::to_string(static_cast<int>(median(firsts)))) + " | " +
          (sizes.empty() ? "-" : std::to_string(static_cast<int>(median(sizes)))) + " | " + c + " |\n";
    Json row = Json::object();
    row["bug"] = lb.id; row["split"] = lb.split; row["runs"] = n; row["detected"] = det; row["valid_reports"] = valid;
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

}  // namespace

int main(int argc, char** argv) {
  std::string mode = argc > 1 ? argv[1] : "";
  std::string lock_path = "data/eval_lock.json", out_dir;
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
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
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

  Json out = Json::object();
  out["tester"] = "random_baseline";
  out["lock_hash"] = hex(lock.hash);
  out["budget_steps"] = static_cast<double>(lock.max_steps);
  Json bugs = Json::array();
  std::string md = "| bug | split | found | median scenarios to first violation | rules |\n|---|---|---|---|---|\n";
  for (const auto& lb : lock.bugs) {
    int idx = bug_index(lb.id);
    if (idx < 0) { std::cerr << "unknown bug in lock: " << lb.id << "\n"; return 2; }
    Json row = Json::object();
    row["bug"] = lb.id;
    row["split"] = lb.split;
    Json runs = Json::array();
    int found = 0;
    std::vector<int> firsts;
    std::string rules;
    for (uint64_t seed : lock.test_seeds) {
      Json r = run_one(bug_bit(idx), seed, budget);
      if (r.at("found").boolean()) {
        ++found;
        firsts.push_back(static_cast<int>(r.at("scenarios_until_first").num()));
        if (rules.find(r.at("rule").str()) == std::string::npos) rules += (rules.empty() ? "" : ", ") + r.at("rule").str();
      }
      runs.push(r);
    }
    row["runs"] = runs;
    row["found_count"] = found;
    row["seed_count"] = static_cast<int>(lock.test_seeds.size());
    std::sort(firsts.begin(), firsts.end());
    char med[16] = "-";
    if (!firsts.empty()) std::snprintf(med, sizeof med, "%d", firsts[firsts.size() / 2]);
    bugs.push(row);
    md += "| " + lb.id + " | " + lb.split + " | " + std::to_string(found) + "/" + std::to_string(lock.test_seeds.size()) +
          " | " + med + " | " + rules + " |\n";
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
