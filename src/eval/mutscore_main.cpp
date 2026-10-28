// rc_mutscore: classifies the mutants of one domain and scores the rule sets on them.
//   rc_mutscore --dir /tmp/mut/orbit --domain orbit --out results/mutation/orbit [--budget-steps 300000]
// Reads clean.tsv, index.json and per-mutant .status/.exit/.tsv written by scripts/run_mutation.sh (see src/eval/mut_eval_main.cpp for the format).
// Classes: compile_failed | crashed_or_timed_out | unchanged (identical traces on the whole pool) | changed_no_rule_fires | killed_by_rules.
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include "eval/ground_truth.hpp"

using namespace rc;

struct Row { int steps = 0; std::string hash, rules, inv; };

static std::vector<Row> load(const std::string& p) {
  std::vector<Row> out;
  std::ifstream f(p);
  for (std::string l; std::getline(f, l);) {
    std::vector<std::string> c;
    std::stringstream ss(l);
    for (std::string t; std::getline(ss, t, '\t');) c.push_back(t);
    if (c.size() < 5) continue;
    Row r; r.steps = std::atoi(c[1].c_str()); r.hash = c[2]; r.rules = c[3]; r.inv = c[4];
    out.push_back(r);
  }
  return out;
}
static std::string first_line(const std::string& p) { std::ifstream f(p); std::string s; std::getline(f, s); return s; }

// rc_mutscore total DIR...   : sums the per-domain mutation.json files into one table (every aggregate in docs/MUTATION.md comes from here)
static int total(int argc, char** argv) {
  std::cout << "| domain | mutants | unchanged on the pool | crashed or timed out | changed, no declared rule fires | killed by hand-written rules | of the changed: killed by hand-written rules | of the changed: killed by model-written invariants | killed mutants that random testing finds in every step-budget window |\n|---|---|---|---|---|---|---|---|---|\n";
  int T[8] = {0};
  for (int i = 2; i < argc; ++i) {
    Json r = Json::parse(read_file(std::string(argv[i]) + "/mutation.json"));
    const Json& c = r.at("classes");
    int n = static_cast<int>(r.at("mutants").arr().size()), un = static_cast<int>(c.num_or("unchanged", 0)), cr = static_cast<int>(c.num_or("crashed_or_timed_out", 0)), si = static_cast<int>(c.num_or("changed_no_rule_fires", 0)),
        ki = static_cast<int>(c.num_or("killed_by_rules", 0)), ch = static_cast<int>(r.num_or("changed", 0)), kh = static_cast<int>(r.num_or("killed_by_hand_rules", 0)), kv = static_cast<int>(r.num_or("killed_by_invariants", 0)), all_w = 0;
    for (const Json& m : r.at("mutants").arr()) if (m.str_or("class", "") == "killed_by_rules" && m.num_or("windows_found", 0) == m.num_or("windows", -1)) ++all_w;
    std::cout << "| " << r.str_or("domain", "") << " | " << n << " | " << un << " | " << cr << " | " << si << " | " << ki << " | " << kh << "/" << ch << " | " << kv << "/" << ch << " | " << all_w << "/" << ki << " |\n";
    int v[8] = {n, un, cr, si, ki, ch, kv, all_w};
    for (int k = 0; k < 8; ++k) T[k] += v[k];
  }
  std::cout << "| **all** | " << T[0] << " | " << T[1] << " | " << T[2] << " | " << T[3] << " | " << T[4] << " | " << T[4] << "/" << T[5] << " | " << T[6] << "/" << T[5] << " | " << T[7] << "/" << T[4] << " |\n";
  return 0;
}

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "total") return total(argc, argv);
  std::string dir, domain, out;
  long long budget = 300000;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--dir") dir = val(); else if (a == "--domain") domain = val(); else if (a == "--out") out = val(); else if (a == "--budget-steps") budget = std::atoll(val().c_str());
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  std::vector<Row> clean = load(dir + "/clean.tsv");
  int clean_hand = 0, clean_inv = 0;
  for (const Row& r : clean) { clean_hand += r.rules != "-"; clean_inv += r.inv != "-"; }
  Json index = Json::parse(read_file(dir + "/index.json"));
  // windows of >= budget steps, in pool order, like the random baseline's step budget
  std::vector<std::pair<int, int>> windows;
  { int s = 0; long long acc = 0; for (size_t i = 0; i < clean.size(); ++i) { acc += clean[i].steps; if (acc >= budget) { windows.push_back({s, static_cast<int>(i)}); s = static_cast<int>(i) + 1; acc = 0; } } }
  std::map<std::string, int> count;
  Json rows = Json::array();
  std::string md = "| mutant | change | class | scenarios violating a hand-written rule | windows (step budget) where random testing finds it | model-written invariants fire |\n|---|---|---|---|---|---|\n";
  std::map<int, int> win_hist;
  int changed = 0, killed_hand = 0, killed_inv = 0, killed_either = 0;
  for (const Json& m : index.arr()) {
    std::string id = m.str_or("id", ""), cls;
    Json r = Json::object();
    r["id"] = id; r["line"] = m.num_or("line", 0); r["change"] = m.str_or("change", "");
    std::string status = first_line(dir + "/" + id + ".status");
    int hand = 0, inv = 0, found_windows = 0, diff = 0;
    if (status != "compiled") cls = "compile_failed";
    else if (first_line(dir + "/" + id + ".exit") != "0") cls = "crashed_or_timed_out";
    else {
      std::vector<Row> rs = load(dir + "/" + id + ".tsv");
      if (rs.size() != clean.size()) cls = "crashed_or_timed_out";
      else {
        for (size_t i = 0; i < rs.size(); ++i) {  // a violation only counts on a scenario where the clean simulator passes
          diff += rs[i].hash != clean[i].hash; hand += rs[i].rules != "-" && clean[i].rules == "-"; inv += rs[i].inv != "-" && clean[i].inv == "-";
        }
        for (auto& w : windows) { bool f = false; for (int i = w.first; i <= w.second; ++i) f |= rs[static_cast<size_t>(i)].rules != "-" && clean[static_cast<size_t>(i)].rules == "-"; found_windows += f; }
        cls = diff == 0 ? "unchanged" : hand > 0 ? "killed_by_rules" : "changed_no_rule_fires";
        if (hand > 0) ++win_hist[found_windows];
        if (diff > 0) { ++changed; killed_hand += hand > 0; killed_inv += inv > 0; killed_either += hand > 0 || inv > 0; }
      }
    }
    ++count[cls];
    r["class"] = cls; r["scenarios_violating"] = hand; r["windows_found"] = found_windows; r["windows"] = static_cast<int>(windows.size()); r["invariants_fire_in_scenarios"] = inv; r["scenarios_changed"] = diff;
    rows.push(r);
    char b[400];
    std::snprintf(b, sizeof b, "| %s | line %d: %s | %s | %d | %d/%d | %d |\n", id.c_str(), static_cast<int>(m.num_or("line", 0)), m.str_or("change", "").c_str(), cls.c_str(), hand, found_windows, static_cast<int>(windows.size()), inv);
    md += b;
  }
  std::ostringstream sum;
  sum << "\n" << domain << ": " << index.arr().size() << " mutants; clean pool of " << clean.size() << " scenarios had " << clean_hand << " hand-rule and " << clean_inv << " invariant violations (both must be 0).\n";
  for (auto& kv : count) sum << "- " << kv.first << ": " << kv.second << "\n";
  sum << "Among mutants whose traces differ from the clean simulator on the pool (" << changed << "): killed by the hand-written rules " << killed_hand << ", by the model-written invariants " << killed_inv
      << ", by either " << killed_either << ".\n";
  sum << "Killed mutants by number of step-budget windows in which random testing finds them (of " << windows.size() << "): ";
  for (auto& kv : win_hist) sum << kv.first << " windows: " << kv.second << "; ";
  sum << "\n";
  Json o = Json::object();
  o["domain"] = domain; o["mutants"] = rows; o["clean_pool"] = static_cast<int>(clean.size()); o["clean_hand_violations"] = clean_hand; o["clean_invariant_violations"] = clean_inv;
  o["changed"] = changed; o["killed_by_hand_rules"] = killed_hand; o["killed_by_invariants"] = killed_inv; o["killed_by_either"] = killed_either; o["windows"] = static_cast<int>(windows.size());
  Json cj = Json::object();
  for (auto& kv : count) cj[kv.first] = kv.second;
  o["classes"] = cj;
  if (std::system(("mkdir -p '" + out + "'").c_str()) != 0) return 1;
  std::ofstream(out + "/mutation.json") << o.dump() << "\n";
  std::ofstream(out + "/mutation.md") << md << sum.str();
  std::cout << md << sum.str();
  return 0;
}
