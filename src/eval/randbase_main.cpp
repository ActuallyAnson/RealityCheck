// rc_randbase: the exact random-testing baseline for the mutation benchmark, with no model and no sampling noise.
// A mutant that makes a hand-written rule fire in k of N pool scenarios is found by n independent random scenarios with probability 1-(1-k/N)^n.
// Prints, per domain and budget, how many mutants have headroom (random testing finds them with probability below 0.5), which is where a smarter search could win.
//   rc_randbase results/mutation/DOMAIN/mutation.json...
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "util/json.hpp"

using namespace rc;

int main(int argc, char** argv) {
  const int budgets[] = {10, 30, 100};
  std::printf("| domain | mutants | behavior changed | rule can fire | headroom at 10 | headroom at 30 | headroom at 100 | expected finds at 10 | at 30 | at 100 |\n|---|---|---|---|---|---|---|---|---|---|\n");
  for (int a = 1; a < argc; ++a) {
    std::ifstream f(argv[a]);
    if (!f) { std::fprintf(stderr, "cannot read %s\n", argv[a]); return 2; }
    std::stringstream ss; ss << f.rdbuf();
    Json j = Json::parse(ss.str());
    const double pool = j.at("clean_pool").num();
    int total = 0, changed = 0, fires = 0, headroom[3] = {0, 0, 0};
    double expect[3] = {0, 0, 0};
    for (const Json& m : j.at("mutants").arr()) {
      ++total;
      if (m.at("scenarios_changed").num() > 0) ++changed;
      const double p = m.at("scenarios_violating").num() / pool;
      if (p <= 0) continue;
      ++fires;
      for (int b = 0; b < 3; ++b) {
        const double hit = 1.0 - std::pow(1.0 - p, budgets[b]);
        expect[b] += hit;
        if (hit < 0.5) ++headroom[b];
      }
    }
    std::string name = j.has("domain") ? j.at("domain").str() : std::string(argv[a]);
    std::printf("| %s | %d | %d | %d | %d | %d | %d | %.1f | %.1f | %.1f |\n", name.c_str(), total, changed, fires, headroom[0], headroom[1], headroom[2], expect[0], expect[1], expect[2]);
  }
  return 0;
}
