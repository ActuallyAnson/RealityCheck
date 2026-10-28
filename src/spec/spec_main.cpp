// rc_spec: the model proposes invariants from a plain-language spec; the engine validates them on clean runs (one
// revision round shows it the counterexamples), then the survivors are used to hunt for each seeded bug.
//   rc_spec --domain orbit --live --out results/spec_orbit [--record CACHE]     |     rc_spec --domain orbit --replay CACHE ...
#include <sys/stat.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "eval/ground_truth.hpp"
#include "model/model.hpp"
#include "rules/checker.hpp"
#include "sim/domains.hpp"
#include "spec/spec.hpp"

using namespace rc;

namespace {
struct Verdict { bool ok = true; std::string evidence; };

// Runs every invariant on `n` clean scenarios (seeds base..base+n-1); returns per-invariant first counterexample.
std::vector<Verdict> validate_clean(const std::vector<Invariant>& invs, const std::string& domain, uint64_t base, int n) {
  std::vector<Verdict> out(invs.size());
  for (int i = 0; i < n; ++i) {
    Scenario sc = random_scenario_for(domain, base + static_cast<uint64_t>(i));
    Signals s = extract_signals(sc, simulate(sc, 0));
    for (size_t k = 0; k < invs.size(); ++k) {
      if (!out[k].ok) continue;
      InvFail f = check_invariant(invs[k], s);
      if (f.failed) {
        std::ostringstream e;
        e << "clean scenario seed " << (base + static_cast<uint64_t>(i)) << ", step " << f.step << ": observed " << f.observed << " vs " << f.limit;
        out[k].ok = false; out[k].evidence = e.str();
      }
    }
  }
  return out;
}

std::string ask(BudgetedModel& model, const ApiConfig& api, std::vector<ChatMessage>& msgs, const std::string& label) {
  ModelRequest rq;
  rq.model = api.model; rq.max_tokens = 12000; rq.reasoning_effort = "low"; rq.messages = msgs;
  ModelResponse r = model.complete(rq, label);
  if (!r.ok) throw std::runtime_error("model error: " + r.error);
  msgs.push_back(r.message);
  return r.message.content;
}
}  // namespace

// rc_spec summarize DIR...  : markdown table from saved spec.json files (every number in docs/SPEC_RULES.md comes from here)
static int summarize(int argc, char** argv) {
  std::cout << "| domain | invariants kept (clean + held-out) | rejected on clean runs | bug | model-written invariants fired | hand-written rules fired | invariants that fired |\n|---|---|---|---|---|---|---|\n";
  int tot = 0, inv_found = 0, hand_found = 0;
  double cost = 0;
  for (int i = 2; i < argc; ++i) {
    Json r = Json::parse(read_file(std::string(argv[i]) + "/spec.json"));
    cost += r.num_or("cost_usd", 0);
    bool first = true;
    for (const Json& b : r.at("bugs").arr()) {
      int sc = static_cast<int>(b.at("scenarios").num()), f = static_cast<int>(b.at("attributable").num()), h = static_cast<int>(b.at("hand_rules_fired").num());
      ++tot; if (f > 0) ++inv_found; if (h > 0) ++hand_found;
      std::string ids;
      for (auto& kv : b.at("by_invariant").obj()) ids += (ids.empty() ? "" : ", ") + kv.first;
      std::cout << "| " << (first ? r.at("domain").str() : "") << " | " << (first ? std::to_string(r.at("accepted").arr().size()) : "") << " | "
                << (first ? std::to_string(r.at("rejected_on_clean").arr().size()) : "") << " | " << b.at("bug").str() << " | " << f << "/" << sc << " | " << h << "/" << sc << " | " << ids << " |\n";
      first = false;
    }
  }
  std::cout << "\nBugs hit at least once in the hunt: model-written invariants " << inv_found << "/" << tot << ", hand-written rules " << hand_found << "/" << tot << ". Model cost $" << cost << ".\n";
  return 0;
}

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "summarize") return summarize(argc, argv);
  std::string domain = "orbit", replay, record, out = "runs/spec", config = "configs/hosted.json";
  bool live = false;
  int language = 1, clean_n = 1500, heldout_n = 3000, hunt_n = 300;
  double budget = 0.5;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--domain") domain = val(); else if (a == "--live") live = true; else if (a == "--replay") replay = val();
    else if (a == "--language") language = std::atoi(val().c_str()); else if (a == "--record") record = val(); else if (a == "--out") out = val(); else if (a == "--config") config = val();
    else if (a == "--clean-n") clean_n = std::atoi(val().c_str()); else if (a == "--heldout-n") heldout_n = std::atoi(val().c_str());
    else if (a == "--hunt-n") hunt_n = std::atoi(val().c_str()); else if (a == "--budget-usd") budget = std::atof(val().c_str());
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  if (!live && replay.empty()) { std::cerr << "choose --live or --replay CACHE\n"; return 2; }
  if (std::system(("mkdir -p '" + out + "'").c_str()) != 0) return 1;

  ApiConfig api = ApiConfig::from_json(Json::parse(read_file(config)));
  std::unique_ptr<ModelBackend> inner, backend;
  if (live) {
    inner.reset(new MetaApiBackend(api));
    if (!record.empty()) backend.reset(new RecordingBackend(*inner, record)); else backend = std::move(inner);
  } else backend.reset(new ReplayBackend(replay));
  BudgetedModel model(*backend, api.pricing, budget, out + "/cost.jsonl");
  if (live) model.set_lifetime_ledger(default_ledger_path(), default_lifetime_cap_usd());

  std::vector<ChatMessage> msgs;
  msgs.push_back({"user", spec_prompt(domain, language), {}, "", {}});
  std::vector<Invariant> accepted, rejected_all;
  std::vector<std::string> problems, rejected_evidence;
  try {
    std::string ans = ask(model, api, msgs, "spec_propose");
    std::vector<Invariant> cand = parse_invariants(ans, domain, &problems, language);
    int proposed = static_cast<int>(cand.size());
    if (cand.empty()) { std::cerr << "no invariants parsed from the answer\n"; return 1; }
    for (int round = 1; round <= 2; ++round) {
      std::vector<Verdict> v = validate_clean(cand, domain, 1, clean_n);
      std::vector<Invariant> rej;
      std::vector<std::string> ev;
      for (size_t k = 0; k < cand.size(); ++k) {
        if (v[k].ok) accepted.push_back(cand[k]); else { rej.push_back(cand[k]); ev.push_back(v[k].evidence); rejected_all.push_back(cand[k]); rejected_evidence.push_back(v[k].evidence); }
      }
      std::cout << "round " << round << ": " << cand.size() << " candidates, " << (cand.size() - rej.size()) << " held on " << clean_n << " clean runs\n";
      if (round == 2 || rej.empty()) break;
      msgs.push_back({"user", revision_prompt(rej, ev), {}, "", {}});
      cand = parse_invariants(ask(model, api, msgs, "spec_revise"), domain, &problems, language);
      for (Invariant& c : cand) { c.id += "_r2"; }
      proposed += static_cast<int>(cand.size());
    }
    std::cout << "proposed " << proposed << " in total, accepted " << accepted.size() << "\n";
  } catch (const std::exception& e) { std::cerr << e.what() << "\n"; return 1; }

  // fresh clean runs: anything that still fails is dropped and reported
  std::vector<Verdict> hv = validate_clean(accepted, domain, 1000000, heldout_n);
  std::vector<Invariant> final_set;
  Json dropped = Json::array();
  for (size_t k = 0; k < accepted.size(); ++k) {
    if (hv[k].ok) final_set.push_back(accepted[k]);
    else { Json d = to_json(accepted[k]); d["evidence"] = hv[k].evidence; dropped.push(d); }
  }

  Json report = Json::object();
  report["domain"] = domain; report["language"] = language; report["model"] = api.model;
  report["clean_validation_runs"] = clean_n; report["heldout_clean_runs"] = heldout_n; report["hunt_scenarios_per_bug"] = hunt_n;
  Json acc = Json::array(), rej = Json::array(), prob = Json::array();
  for (const Invariant& i : final_set) acc.push(to_json(i));
  for (size_t k = 0; k < rejected_all.size(); ++k) { Json d = to_json(rejected_all[k]); d["evidence"] = rejected_evidence[k]; rej.push(d); }
  for (const std::string& p : problems) prob.push(p);
  report["accepted"] = acc; report["rejected_on_clean"] = rej; report["dropped_on_heldout_clean"] = dropped; report["parse_problems"] = prob;

  // hunt: for each seeded bug, how often and how fast do the surviving invariants (and, for comparison, the hand-written rules) fire?
  Json bugs = Json::array();
  for (int b = 0; b < domain_num_bugs(domain); ++b) {
    BugMask mask = bug_bit(b);
    int inv_hit = 0, inv_attr = 0, hand_hit = 0, first_inv = 0, first_hand = 0;
    std::map<std::string, int> by_inv;
    for (int i = 0; i < hunt_n; ++i) {
      Scenario sc = random_scenario_for(domain, 2000000 + static_cast<uint64_t>(i));
      Trace tr = simulate(sc, mask);
      Signals s = extract_signals(sc, tr), sc_clean;
      bool hit = false, attr = false, clean_done = false;
      for (const Invariant& inv : final_set) {
        if (!check_invariant(inv, s).failed) continue;
        hit = true;
        if (!clean_done) { sc_clean = extract_signals(sc, simulate(sc, 0)); clean_done = true; }
        if (!check_invariant(inv, sc_clean).failed) { attr = true; by_inv[inv.id]++; }
      }
      if (hit) { ++inv_hit; if (!first_inv) first_inv = i + 1; }
      if (attr) ++inv_attr;
      if (!check_rules(sc, tr).empty()) { ++hand_hit; if (!first_hand) first_hand = i + 1; }
    }
    Json o = Json::object();
    o["bug"] = domain_bug_name(domain, b);
    o["scenarios"] = hunt_n; o["invariants_fired"] = inv_hit; o["attributable"] = inv_attr; o["hand_rules_fired"] = hand_hit;
    o["first_scenario_invariants"] = first_inv; o["first_scenario_hand_rules"] = first_hand;
    Json bi = Json::object();
    for (auto& kv : by_inv) bi[kv.first] = kv.second;
    o["by_invariant"] = bi;
    bugs.push(o);
    std::cout << o.at("bug").str() << ": invariants fired on " << inv_hit << "/" << hunt_n << " (attributable " << inv_attr << "), hand-written rules " << hand_hit << "/" << hunt_n << "\n";
  }
  report["bugs"] = bugs;
  report["cost_usd"] = model.spent();
  std::ofstream(out + "/spec.json") << report.dump() << "\n";
  std::cout << "accepted " << final_set.size() << " invariants after held-out clean check; cost $" << model.spent() << "\n";
  return 0;
}
