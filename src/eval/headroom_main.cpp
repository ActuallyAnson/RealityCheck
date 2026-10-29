// rc_headroom: the pre-registered agent-versus-random test (docs/PREREG_FINAL.md, section 2).
//   rc_headroom AGENT_RUNS_DIR:RANDOM_BASELINE_JSON ...
// For every bug in the agent runs, compares the agent's detection count with the random baseline's at the same step budget (one-sided Fisher exact test, agent > random), reports valid reports,
// and then an exact one-sided sign test over the bugs whose detection rates differ.
#include <dirent.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>

#include "util/json.hpp"

using namespace rc;

static std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
static double lchoose(int n, int k) { return std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0); }
// P(X >= a) for the 2x2 table [[a, n1-a], [b, n2-b]] under the hypergeometric null (row 1 = agent).
static double fisher_greater(int a, int n1, int b, int n2) {
  const int K = a + b, N = n1 + n2;
  double p = 0;
  for (int x = a; x <= std::min(n1, K); ++x) p += std::exp(lchoose(K, x) + lchoose(N - K, n1 - x) - lchoose(N, n1));
  return std::min(1.0, p);
}

int main(int argc, char** argv) {
  std::printf("| bug | agent detected | agent valid report | random detected | one-sided Fisher p (agent > random) |\n|---|---|---|---|---|\n");
  int wins = 0, losses = 0, ties = 0, ta = 0, na = 0, tr = 0, nr = 0, tv = 0;
  for (int a = 1; a < argc; ++a) {
    std::string arg = argv[a];
    size_t c = arg.find(':');
    const std::string dir = arg.substr(0, c), base = arg.substr(c + 1);
    std::map<std::string, std::pair<int, int>> det, val;  // bug -> (count, runs)
    DIR* d = opendir((dir + "/runs").c_str());
    if (!d) { std::fprintf(stderr, "no runs in %s\n", dir.c_str()); return 2; }
    while (dirent* e = readdir(d)) {
      std::string n = e->d_name;
      if (n.size() < 6 || n.substr(n.size() - 5) != ".json") continue;
      Json r = Json::parse(slurp(dir + "/runs/" + n));
      const std::string bug = r.str_or("bug", "");
      ++det[bug].second; ++val[bug].second;
      det[bug].first += r.bool_or("detected", false); val[bug].first += r.bool_or("report_valid", false);
    }
    closedir(d);
    Json b = Json::parse(slurp(base));
    for (const Json& bb : b.at("bugs").arr()) {
      const std::string bug = bb.str_or("bug", "");
      if (!det.count(bug)) continue;
      const int k = static_cast<int>(bb.num_or("found_count", 0)), n = static_cast<int>(bb.at("runs").arr().size());
      const int ak = det[bug].first, an = det[bug].second;
      const double p = fisher_greater(ak, an, k, n);
      std::printf("| %s | %d/%d | %d/%d | %d/%d | %.4f |\n", bug.c_str(), ak, an, val[bug].first, an, k, n, p);
      const double ra = static_cast<double>(ak) / an, rr = static_cast<double>(k) / n;
      if (ra > rr) ++wins; else if (ra < rr) ++losses; else ++ties;
      ta += ak; na += an; tr += k; nr += n; tv += val[bug].first;
    }
  }
  double tail = 0, cc = 1;
  const int m = wins + losses;
  for (int j = wins; j <= m; ++j) { tail += std::exp(lchoose(m, j)); }
  (void)cc;
  std::printf("\nAll targets: agent detected %d/%d (valid reports %d), random %d/%d. Bugs where the agent's rate is higher: %d, lower: %d, equal: %d; exact one-sided sign test p = %.4f.\n", ta, na, tv, tr, nr, wins, losses, ties, m ? tail / std::pow(2.0, m) : 1.0);
  return 0;
}
