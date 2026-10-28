// minescore: scores the invariant-mining arm on a finished mutation run (mut_eval --mine ...; the TSV's 7th column is the mined invariants that fired).
//   rc_minescore --dir /tmp/mut_mine/orbit --mutation results/mutation/orbit/mutation.json --domain orbit
// A mutant counts as killed by the miner when some scenario fires a mined invariant that the unmodified simulator's mined invariants do not fire on, and its trace differs from the clean one.
#include <fstream>
#include <iostream>
#include <sstream>

#include "util/json.hpp"

using namespace rc;

namespace {
struct Row { std::string hash, mined; };
std::vector<Row> load(const std::string& p) {
  std::vector<Row> out;
  std::ifstream f(p);
  for (std::string l; std::getline(f, l);) {
    std::vector<std::string> c; std::stringstream ss(l);
    for (std::string t; std::getline(ss, t, '\t');) c.push_back(t);
    if (c.size() >= 7) out.push_back({c[2], c[6]});
  }
  return out;
}
}  // namespace

int main(int argc, char** argv) {
  std::string dir, mutation, domain;
  for (int i = 1; i + 1 < argc; i += 2) {
    std::string a = argv[i];
    if (a == "--dir") dir = argv[i + 1]; else if (a == "--mutation") mutation = argv[i + 1]; else if (a == "--domain") domain = argv[i + 1];
  }
  if (dir.empty() || mutation.empty()) { std::cerr << "usage: rc_minescore --dir D --mutation mutation.json --domain NAME\n"; return 2; }
  const std::vector<Row> clean = load(dir + "/clean.tsv");
  int clean_fp = 0;
  for (const Row& r : clean) clean_fp += r.mined != "-";
  Json mj = Json::parse(read_file(mutation));
  int changed = 0, kill_mined = 0, kill_old = 0, kill_union = 0, mined_only = 0, silent = 0, silent_caught = 0;
  Json rows = Json::array();
  for (const Json& m : mj.at("mutants").arr()) {
    const std::string cls = m.str_or("class", "");
    const std::string id = m.str_or("id", "");
    const bool old_kill = m.num_or("scenarios_violating", 0) > 0 || m.num_or("invariants_fire_in_scenarios", 0) > 0 || m.num_or("metamorphic_fire_in_scenarios", 0) > 0;
    std::vector<Row> rs = load(dir + "/" + id + ".tsv");
    int hit = 0, diff = 0;
    for (size_t i = 0; i < rs.size() && i < clean.size(); ++i) { diff += rs[i].hash != clean[i].hash; hit += rs[i].mined != "-" && clean[i].mined == "-"; }
    if (rs.empty() || cls == "unchanged" || cls == "crashed_or_timed_out") continue;
    ++changed; kill_old += old_kill; kill_mined += hit > 0; kill_union += old_kill || hit > 0; mined_only += !old_kill && hit > 0;
    if (!old_kill) { ++silent; silent_caught += hit > 0; }
    Json r = Json::object(); r["mutant"] = id; r["change"] = m.str_or("change", ""); r["scenarios_firing_mined"] = hit; r["old_oracles_kill"] = old_kill; rows.push(r);
  }
  std::cout << domain << ": changed mutants " << changed << "; clean pool false-positive scenarios " << clean_fp << " of " << clean.size() << "; killed by mined invariants " << kill_mined
            << "; by hand rules, invariants or metamorphic " << kill_old << "; union " << kill_union << "; mined only " << mined_only << "; of the " << silent << " that none of the old oracles catch the miner catches " << silent_caught << "\n";
  Json o = Json::object();
  o["domain"] = domain; o["changed"] = changed; o["clean_false_positive_scenarios"] = clean_fp; o["clean_pool"] = static_cast<int>(clean.size()); o["killed_by_mined"] = kill_mined; o["killed_by_old_oracles"] = kill_old;
  o["union"] = kill_union; o["mined_only"] = mined_only; o["silent_to_old"] = silent; o["silent_caught_by_mined"] = silent_caught; o["mutants"] = rows;
  std::ofstream(dir + "/minescore.json") << o.dump(2) << "\n";
  return 0;
}
