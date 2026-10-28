// rc_refscore: scores the reference oracle against a locked bug set.
//   rc_refscore mutants MUTATION_JSON WORKDIR      (the 160 sampled mutants of a domain: classes from results/mutation/<domain>/mutation.json)
//   rc_refscore planted BLIND_EVAL_TXT WORKDIR     (the model-planted variants: results/blind/eval_<domain>.txt)
// A variant is "caught by the reference" when at least one scenario has a flagged step. "Silent" = changed behavior and no hand-written rule, model-written invariant or metamorphic relation fires.
#include <fstream>
#include <iostream>
#include <sstream>

#include "util/json.hpp"

using namespace rc;

static std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }

static void scenarios_flagged(const std::string& file, int* checked, int* flagged, bool* ok) {
  std::ifstream f(file);
  *ok = f.good();
  std::string line;
  *checked = *flagged = 0;
  while (std::getline(f, line)) {
    std::istringstream is(line);
    long s; int c, fl;
    if (is >> s >> c >> fl) { *checked += c; *flagged += fl > 0; }
  }
}

int main(int argc, char** argv) {
  if (argc < 4) { std::cerr << "usage: rc_refscore mutants|planted FILE WORKDIR\n"; return 2; }
  const std::string mode = argv[1], dir = argv[3];
  int changed = 0, caught_changed = 0, silent = 0, caught_silent = 0, rule_caught = 0, caught_rule = 0, missing = 0, clean_flag = 0;
  int crashed = 0, caught_crashed = 0;
  auto account = [&](const std::string& id, bool is_changed, bool by_rule, bool silent_to_all, bool crash = false) {
    int checked, flagged; bool ok;
    scenarios_flagged(dir + "/" + id + ".ref.tsv", &checked, &flagged, &ok);
    if (!ok || checked == 0) { ++missing; return; }
    const bool caught = flagged > 0;
    std::printf("%s\t%s\t%d\t%d\t%s\n", id.c_str(), is_changed ? (by_rule ? "rule" : silent_to_all ? "silent" : "other") : "unchanged", checked, flagged, caught ? "CAUGHT" : "-");
    if (crash) { ++crashed; caught_crashed += caught; return; }
    if (!is_changed) { clean_flag += caught; return; }
    ++changed; caught_changed += caught;
    if (by_rule) { ++rule_caught; caught_rule += caught; }
    if (silent_to_all) { ++silent; caught_silent += caught; }
  };
  if (mode == "mutants") {
    Json j = Json::parse(slurp(argv[2]));
    for (const Json& m : j.at("mutants").arr()) {
      const std::string cls = m.str_or("class", "");
      const bool is_changed = cls != "unchanged" && m.num_or("scenarios_changed", 0) > 0;
      const bool by_rule = cls == "killed_by_rules";
      const bool sil = is_changed && !by_rule && m.num_or("invariants_fire_in_scenarios", 0) == 0 && m.num_or("metamorphic_fire_in_scenarios", 0) == 0;
      account(m.str_or("id", ""), is_changed, by_rule, sil, cls == "crashed_or_timed_out");
    }
  } else {
    std::ifstream f(argv[2]);
    std::string line;
    while (std::getline(f, line)) {
      size_t c = line.find(':');
      if (c == std::string::npos) continue;
      const std::string id = line.substr(0, c);
      int diff = 0, hand = 0;
      std::sscanf(line.c_str() + c, ": differs from clean on %d/%*d scenarios; hand-written rule fires on %d", &diff, &hand);
      account(id, diff > 0, hand > 0, diff > 0 && hand == 0);
    }
  }
  std::printf("\nchanged variants scored: %d; caught by the reference: %d\n", changed, caught_changed);
  std::printf("of those a hand-written rule catches: %d, caught by the reference: %d\n", rule_caught, caught_rule);
  std::printf("silent to every existing oracle (rules; for mutants also invariants and metamorphic): %d, caught by the reference: %d\n", silent, caught_silent);
  std::printf("mutants that crashed or timed out in the original evaluation: %d, flagged by the reference: %d\n", crashed, caught_crashed);
  std::printf("unchanged variants flagged (false alarms): %d; variants not scored (build failed, timeout or no output): %d\n", clean_flag, missing);
  return 0;
}
