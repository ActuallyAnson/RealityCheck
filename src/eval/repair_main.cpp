// rc_repair: can a model repair a mutated simulator, and does "the declared rules pass" suffice to accept the repair?  (docs/REPAIR.md)
// For each mutant that a declared rule catches, the model is shown the mutated source regions, one failing scenario and the violated rule, and proposes replacement text for the regions.
// The engine (not the model) then: scans the patch against a denylist, compiles it, runs it SANDBOXED (no network, no file writes, no forking) on a pool of scenarios and classifies the outcome:
//   restored            every trace on the pool is bit-identical to the unmodified simulator's
//   rules_pass_differs  no declared rule or model-written invariant fails, but some trace differs from the unmodified simulator (a rules-only check would accept this)
//   still_violates      a declared rule still fails where the unmodified simulator passes
//   compile_failed | denied | timeout_or_crash
//   rc_repair --domain orbit --file src/sim/orbit.cpp --ranges 74-146,164-189 --mutdir /tmp/mut/orbit --mutation results/mutation/orbit/mutation.json --live --out results/repair/orbit
#include <sys/stat.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "eval/ground_truth.hpp"
#include "model/model.hpp"
#include "rules/checker.hpp"
#include "sim/domains.hpp"
#include "tools/tools.hpp"

using namespace rc;

namespace {
std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
std::vector<std::string> split_lines(const std::string& s) { std::vector<std::string> v; std::stringstream ss(s); for (std::string l; std::getline(ss, l);) v.push_back(l); return v; }
std::vector<std::pair<int, int>> parse_ranges(const std::string& s) {
  std::vector<std::pair<int, int>> out;
  std::stringstream ss(s);
  for (std::string t; std::getline(ss, t, ',');) { size_t d = t.find('-'); out.push_back({std::atoi(t.substr(0, d).c_str()), std::atoi(t.substr(d + 1).c_str())}); }
  return out;
}
struct Row { std::string hash, rules, inv, mr = "-"; };
std::vector<Row> load_tsv(const std::string& p) {
  std::vector<Row> out;
  std::ifstream f(p);
  for (std::string l; std::getline(f, l);) {
    std::vector<std::string> c; std::stringstream ss(l);
    for (std::string t; std::getline(ss, t, '\t');) c.push_back(t);
    if (c.size() >= 5) out.push_back({c[2], c[3], c[4], c.size() > 5 ? c[5] : "-"});
  }
  return out;
}

// Patch text must be plain simulator code: no preprocessor, no process, file, network or dynamic-loading calls, no inline assembly.
std::string denylist_hit(const std::string& code) {
  static const char* bad[] = {"#", "system", "popen", "exec", "fork", "fopen", "freopen", "ofstream", "ifstream", "fstream", "filesystem", "socket", "connect(", "asm", "dlopen", "dlsym", "getenv", "setenv",
                              "unlink", "remove(", "rename(", "kill(", "syscall", "mmap", "chmod", "std::thread", "pthread", "__"};
  for (const char* b : bad) if (code.find(b) != std::string::npos) return b;
  // signal( is the POSIX call unless it is a member call (`s.signal(`) or `int signal(` (traffic.cpp's own accessor)
  for (size_t at = code.find("signal("); at != std::string::npos; at = code.find("signal(", at + 1))
    if (at == 0 || (code[at - 1] != '.' && code.compare(at >= 4 ? at - 4 : 0, 4, "int ") != 0)) return "signal(";
  return "";
}

// First fenced block after a heading such as "REGION 2".
std::string region_block(const std::string& text, int n) {
  std::string head = "REGION " + std::to_string(n);
  size_t p = text.find(head);
  if (p == std::string::npos) return "";
  size_t a = text.find("```", p);
  if (a == std::string::npos) return "";
  a = text.find('\n', a);
  size_t b = text.find("```", a);
  if (a == std::string::npos || b == std::string::npos) return "";
  return text.substr(a + 1, b - a - 1);
}
}  // namespace

int main(int argc, char** argv) {
  std::string domain = "orbit", file, ranges, mutdir, mutation, out = "results/repair", config = "configs/hosted.json", invariants;
  bool live = false;
  std::string selftest;  // original | same | wrong | evil: skip the model and feed a synthetic answer through the same pipeline
  int attempts = 2, limit = 100, pool = 300;
  double budget = 3.0;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--domain") domain = val(); else if (a == "--file") file = val(); else if (a == "--ranges") ranges = val(); else if (a == "--mutdir") mutdir = val();
    else if (a == "--mutation") mutation = val(); else if (a == "--out") out = val(); else if (a == "--config") config = val(); else if (a == "--live") live = true; else if (a == "--selftest") selftest = val();
    else if (a == "--attempts") attempts = std::atoi(val().c_str()); else if (a == "--limit") limit = std::atoi(val().c_str()); else if (a == "--budget-usd") budget = std::atof(val().c_str());
    else if (a == "--invariants") invariants = val(); else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  if ((!live && selftest.empty()) || file.empty() || ranges.empty() || mutdir.empty() || mutation.empty()) { std::cerr << "usage: rc_repair --live --domain D --file F --ranges a-b,c-d --mutdir DIR --mutation mutation.json [--out DIR] [--attempts N] [--limit N] [--budget-usd X]\n"; return 2; }
  if (invariants.empty()) invariants = "results/spec_" + domain + "/spec.json";
  if (std::system(("mkdir -p '" + out + "'").c_str()) != 0) return 1;
  ApiConfig api = ApiConfig::from_json(Json::parse(read_file(config)));
  MetaApiBackend backend(api);
  RecordingBackend rec(backend, out + "/cache.jsonl");
  BudgetedModel model(rec, api.pricing, budget, out + "/cost.jsonl");
  model.set_lifetime_ledger(default_ledger_path(), default_lifetime_cap_usd());
  const auto rg = parse_ranges(ranges);
  const std::vector<Row> clean = load_tsv(mutdir + "/clean.tsv");
  Json mj = Json::parse(read_file(mutation));
  Json results = Json::array();
  int done = 0;
  for (const Json& m : mj.at("mutants").arr()) {
    if (m.str_or("class", "") != "killed_by_rules" || done >= limit) continue;
    const std::string id = m.str_or("id", "");
    std::string failing_json, rule;
    std::vector<Row> mt = load_tsv(mutdir + "/" + id + ".tsv");
    int idx = -1;
    for (size_t i = 0; i < mt.size() && i < clean.size(); ++i) if (mt[i].rules != "-" && clean[i].rules == "-") { idx = static_cast<int>(i); rule = mt[i].rules.substr(0, mt[i].rules.find(',')); break; }
    if (idx < 0) continue;
    Scenario sc = random_scenario_for(domain, 700000 + static_cast<uint64_t>(idx));
    failing_json = to_json(sc).dump();
    ++done;
    const std::vector<std::string> mutated = split_lines(slurp(mutdir + "/" + id + ".cpp"));
    // the regions the model sees (line-numbered) and may replace
    std::ostringstream prompt;
    prompt << "A deterministic simulator in C++ violates one of its declared rules. Your task is to repair its source.\n\n" << simulation_description(domain)
           << "\nThe rule `" << rule << "` is violated on this scenario (the unmodified, correct simulator passes it):\n" << failing_json
           << "\n\nSOURCE (file " << file << "). You may change only the lines inside the numbered regions below; everything else is fixed.\n";
    for (size_t r = 0; r < rg.size(); ++r) {
      prompt << "\nREGION " << (r + 1) << " (lines " << rg[r].first << " to " << rg[r].second << "):\n```\n";
      for (int l = rg[r].first; l <= rg[r].second && l <= static_cast<int>(mutated.size()); ++l) prompt << mutated[static_cast<size_t>(l - 1)] << "\n";
      prompt << "```\n";
    }
    prompt << "\nReturn, for every region, the complete corrected text of that region (all of its lines, repaired where needed) in a fenced block that follows a line `REGION n`. Do not add #include or #define lines, and do not call anything that touches the operating system. Keep the code deterministic.";
    std::vector<ChatMessage> msgs;
    msgs.push_back({"user", prompt.str(), {}, "", {}});
    Json row = Json::object();
    row["mutant"] = id; row["line"] = m.num_or("line", 0); row["change"] = m.str_or("change", ""); row["rule"] = rule; row["failing_scenario_index"] = idx;
    std::string outcome = "no_attempt";
    Json tries = Json::array();
    for (int at = 1; at <= attempts; ++at) {
      Json t = Json::object();
      t["attempt"] = at;
      ModelRequest rq; rq.model = api.model; rq.max_tokens = 10000; rq.reasoning_effort = "low"; rq.messages = msgs;
      ModelResponse resp;
      if (!selftest.empty()) {  // synthetic answer built from the unmodified source (original), a plausible but wrong edit (wrong), or a patch that tries to run a command (evil)
        const std::vector<std::string> orig = selftest == "same" ? mutated : split_lines(slurp(file));  // same: return the mutant unchanged
        std::string text;
        for (size_t r = 0; r < rg.size(); ++r) {
          text += "REGION " + std::to_string(r + 1) + "\n```\n";
          for (int l = rg[r].first; l <= rg[r].second; ++l) {
            std::string line = orig[static_cast<size_t>(l - 1)];
            if (selftest == "wrong" && r == 0 && l == rg[r].first + 6) line += " // wrong edit below\n  if (n > 0) h *= 1.5;";
            if (selftest == "evil" && r == 0 && l == rg[r].first + 6) line += "\n  std::system(\"touch /tmp/rc_repair_pwned\");";
            text += line + "\n";
          }
          text += "```\n";
        }
        resp.ok = true; resp.message.role = "assistant"; resp.message.content = text;
      } else
      try { resp = model.complete(rq, "repair"); } catch (const BudgetExceeded&) { outcome = "budget"; break; }
      if (!resp.ok) { outcome = "model_error"; t["error"] = resp.error; tries.push(t); break; }
      msgs.push_back(resp.message);
      std::vector<std::string> patched = mutated;
      std::string feedback;
      std::vector<std::string> blocks;
      for (size_t r = 0; r < rg.size(); ++r) blocks.push_back(region_block(resp.message.content, static_cast<int>(r + 1)));
      bool missing = false;
      for (auto& b : blocks) missing |= b.empty();
      if (missing) { outcome = "unparsed"; feedback = "Your answer did not contain a fenced block for every region."; }
      else {
        std::string hit;
        for (auto& b : blocks) { std::string h = denylist_hit(b); if (!h.empty()) hit = h; }
        if (!hit.empty()) { outcome = "denied"; feedback = "The patch was refused: it contains '" + hit + "'. Plain simulator code only."; t["denied_token"] = hit; }
        else {
          // splice from the last region to the first so line numbers stay valid
          for (size_t r = rg.size(); r-- > 0;) {
            std::vector<std::string> nl = split_lines(blocks[r]);
            patched.erase(patched.begin() + (rg[r].first - 1), patched.begin() + rg[r].second);
            patched.insert(patched.begin() + (rg[r].first - 1), nl.begin(), nl.end());
          }
          std::string pid = "p" + id.substr(1) + "_" + std::to_string(at);
          std::ofstream(mutdir + "/" + pid + ".cpp") << [&] { std::string s; for (auto& l : patched) s += l + "\n"; return s; }();
          std::string cmd = "RC_SANDBOX=1 MR_N=100 sh scripts/mutation_worker.sh '" + mutdir + "' " + pid + " " + domain + " " + invariants + " " + std::to_string(pool) + " 700000 >/dev/null 2>&1";
          int rc = std::system(cmd.c_str());
          (void)rc;
          std::string status = slurp(mutdir + "/" + pid + ".status");
          if (status.find("compiled") == std::string::npos) {
            outcome = "compile_failed";
            std::string log = slurp(mutdir + "/" + pid + ".cc.log");
            std::string first;
            { std::stringstream ss(log); for (std::string l; std::getline(ss, l) && first.size() < 700;) if (l.find("error") != std::string::npos) first += l + "\n"; }
            feedback = "It did not compile:\n" + first;
          } else {
            std::string ex = slurp(mutdir + "/" + pid + ".exit");
            std::vector<Row> pr = load_tsv(mutdir + "/" + pid + ".tsv");
            if (ex.find('0') != 0 || pr.size() < static_cast<size_t>(pool)) { outcome = "timeout_or_crash"; feedback = "Running it crashed or timed out."; }
            else {
              int viol = -1, differs = 0, inv = 0, mr = 0;
              for (size_t i = 0; i < pr.size() && i < clean.size(); ++i) {
                if (pr[i].rules != "-" && clean[i].rules == "-" && viol < 0) viol = static_cast<int>(i);
                differs += pr[i].hash != clean[i].hash;
                inv += pr[i].inv != "-" && clean[i].inv == "-";
                mr += pr[i].mr != "-" && clean[i].mr == "-";
              }
              t["scenarios_differing_from_unmodified"] = differs; t["invariant_failures"] = inv; t["metamorphic_failures"] = mr;
              if (viol >= 0) { outcome = "still_violates"; feedback = "The patched simulator still violates " + pr[static_cast<size_t>(viol)].rules + " on scenario seed " + std::to_string(700000 + viol) + "."; }
              else if (differs == 0) outcome = "restored";
              else outcome = "rules_pass_differs";
            }
          }
          if (std::system(("rm -f '" + mutdir + "/" + pid + "'.* 2>/dev/null").c_str()) != 0) {}
          if (outcome == "restored" || outcome == "rules_pass_differs") { t["outcome"] = outcome; tries.push(t); break; }
        }
      }
      t["outcome"] = outcome;
      tries.push(t);
      if (at < attempts) msgs.push_back({"user", feedback + " Please return corrected text for every region.", {}, "", {}});
    }
    row["outcome"] = outcome; row["attempts"] = tries;
    results.push(row);
    std::cout << id << " (" << m.str_or("change", "") << "): " << outcome << "\n" << std::flush;
    std::ofstream(out + "/repair.json") << results.dump() << "\n";
    if (outcome == "budget") break;
  }
  std::map<std::string, int> count;
  for (const Json& r : results.arr()) ++count[r.str_or("outcome", "")];
  std::cout << "summary:"; for (auto& kv : count) std::cout << " " << kv.first << "=" << kv.second; std::cout << "  cost $" << model.spent() << "\n";
  return 0;
}
