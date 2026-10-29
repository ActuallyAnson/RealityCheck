// rc_gap: the oracle-gap table behind the viewer's "Oracle gap" screen (web/data/gap.json), computed from result files only; no simulation, no model.
//   rc_gap DOMAIN=REFERENCE_MUTANT_DIR...    (REFERENCE_MUTANT_DIR holds ID.ref.tsv from scripts/refcheck_worker.sh, or "-" if no reference oracle was run for the domain)
// For each domain, over the 40 sampled mutants of results/mutation/DOMAIN/mutation.json: how many change behavior, how many a hand-written rule catches, how many only a model-written
// invariant or metamorphic relation adds, how many only the reference oracle adds (a changed mutant that no other oracle fires on and the reference flags), and how many stay silent.
#include <fstream>
#include <iostream>
#include <sstream>

#include "util/json.hpp"

using namespace rc;

static std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }

int main(int argc, char** argv) {
  Json out = Json::object();
  Json rows = Json::array();
  for (int a = 1; a < argc; ++a) {
    std::string arg = argv[a];
    size_t eq = arg.find('=');
    if (eq == std::string::npos) { std::cerr << "usage: rc_gap DOMAIN=DIR|-...\n"; return 2; }
    const std::string dom = arg.substr(0, eq), dir = arg.substr(eq + 1);
    Json m = Json::parse(slurp("results/mutation/" + dom + "/mutation.json"));
    int changed = 0, rules = 0, any = 0, ref_extra = 0, crashed = 0, ref_scored = 0;
    for (const Json& x : m.at("mutants").arr()) {
      const std::string cls = x.str_or("class", "");
      if (cls == "crashed_or_timed_out") { ++crashed; continue; }
      if (cls == "unchanged" || x.num_or("scenarios_changed", 0) <= 0) continue;
      ++changed;
      const bool by_rule = cls == "killed_by_rules";
      const bool other = x.num_or("invariants_fire_in_scenarios", 0) > 0 || x.num_or("metamorphic_fire_in_scenarios", 0) > 0;
      rules += by_rule;
      any += by_rule || other;
      if (!by_rule && !other && dir != "-") {
        std::ifstream f(dir + "/" + x.str_or("id", "") + ".ref.tsv");
        std::string line; long checked = 0, flagged = 0;
        while (std::getline(f, line)) { std::istringstream is(line); long s; int c, fl; if (is >> s >> c >> fl) { checked += c; flagged += fl > 0; } }
        if (checked > 0) { ++ref_scored; ref_extra += flagged > 0; }
      }
    }
    Json r = Json::object();
    r["domain"] = dom; r["sampled"] = static_cast<double>(m.at("mutants").arr().size()); r["changed"] = changed; r["rules"] = rules; r["other_existing"] = any - rules;
    r["reference_run"] = dir != "-"; r["reference_extra"] = ref_extra; r["silent"] = changed - any - ref_extra; r["crashed_or_timed_out"] = crashed;
    rows.push(r);
  }
  out["domains"] = rows;
  out["note"] = "For each simulator, the sampled bugs (mechanical mutations of its source, results/mutation) whose behavior changed, split by which check notices them. "
                "A bug is 'unseen' when it changes what the simulator does and no check fires on it. Unseen does not always mean harmful: some changes may be harmless numeric differences. Bugs that crashed or hung are left out.";
  std::cout << out.dump() << "\n";
  return 0;
}
