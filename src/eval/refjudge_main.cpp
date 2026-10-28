// refjudge: the trusted half of the reference oracle. Reads the dump (what the system under test recorded) and the reference's predictions (refrun) in lockstep
// and flags a step when some part of the state of the recording differs from the prediction by more than the tolerance (orbit: per body, position and velocity as 2-vectors; cart-pole: each component):
//     tol = max( F * |recorded change over the step|,  K * |reference one-step - reference two half-steps|,  1e-9 * (1 + |value|) )
// F (relative tolerance, default 0.02) and K (default 10) are fixed before any run and are not fitted to the clean simulator. The second term is the reference's own error estimate.
// For cart-pole the sign of the applied force is not recorded; the sign whose prediction fits better is used.
//   refjudge --domain orbit|cartpole DUMP PRED   -> TSV: scenario, steps checked, steps flagged, first flagged step, worst error/tolerance
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  std::string domain = "orbit", dump, pred;
  double F = 0.02, K = 10;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--domain" && i + 1 < argc) domain = argv[++i]; else if (a == "--rel" && i + 1 < argc) F = std::atof(argv[++i]);
    else if (a == "--k" && i + 1 < argc) K = std::atof(argv[++i]); else if (dump.empty()) dump = a; else pred = a;
  }
  std::ifstream fd(dump), fp(pred);
  std::string ld, lp;
  struct Acc { int checked = 0, flagged = 0, first = -1; double worst = 0; };
  std::map<long, Acc> by;
  long total = 0, flagged_total = 0;
  while (std::getline(fd, ld)) {
    if (!std::getline(fp, lp)) break;  // the reference produced fewer lines (crashed or timed out): the rest are unjudged
    std::istringstream d(ld), p(lp);
    long s, k, s2, k2;
    d >> s >> k; p >> s2 >> k2;
    if (s != s2 || k != k2) continue;
    std::vector<double> before, after, full, half;
    double worst = 0;
    if (domain == "orbit") {
      double dt, G; int n;
      d >> dt >> G >> n;
      std::vector<double> m(static_cast<size_t>(n)); for (auto& v : m) d >> v;
      before.resize(static_cast<size_t>(4 * n)); after.resize(static_cast<size_t>(4 * n));
      for (auto& v : before) d >> v;
      for (auto& v : after) d >> v;
      int n2; p >> n2;
      full.resize(static_cast<size_t>(4 * n)); half.resize(static_cast<size_t>(4 * n));
      for (auto& v : full) p >> v;
      for (auto& v : half) p >> v;
      // per body, positions and velocities are compared as 2-vectors: a single component that happens to barely change would make a componentwise tolerance meaningless
      for (int b = 0; b < n; ++b) {
        for (int part = 0; part < 2; ++part) {
          size_t i0 = static_cast<size_t>(4 * b + 2 * part);
          auto norm = [&](const std::vector<double>& u, const std::vector<double>& v) { return std::hypot(u[i0] - v[i0], u[i0 + 1] - v[i0 + 1]); };
          double scale = std::max(norm(after, before), 0.0), mag = std::hypot(after[i0], after[i0 + 1]);
          double tol = std::max({F * scale, K * norm(full, half), 1e-9 * (1 + mag)});
          worst = std::max(worst, norm(after, full) / tol);
        }
      }
    } else {
      double dt, mc, mp, l, g, fmag;
      d >> dt >> mc >> mp >> l >> g >> fmag;
      before.resize(4); after.resize(4);
      for (auto& v : before) d >> v;
      for (auto& v : after) d >> v;
      worst = 1e300;
      for (int sign = 0; sign < 2; ++sign) {
        full.resize(4); half.resize(4);
        for (auto& v : full) p >> v;
        for (auto& v : half) p >> v;
        double w = 0;
        for (size_t j = 0; j < 4; ++j) {
          double tol = std::max({F * std::fabs(after[j] - before[j]), K * std::fabs(full[j] - half[j]), 1e-9 * (1 + std::fabs(after[j]))});
          w = std::max(w, std::fabs(after[j] - full[j]) / tol);
        }
        worst = std::min(worst, w);
      }
    }
    Acc& a = by[s];
    ++a.checked; ++total;
    a.worst = std::max(a.worst, worst);
    if (!(worst <= 1.0)) { ++a.flagged; ++flagged_total; if (a.first < 0) a.first = static_cast<int>(k); }
  }
  for (const auto& kv : by) std::printf("%ld\t%d\t%d\t%d\t%.4g\n", kv.first, kv.second.checked, kv.second.flagged, kv.second.first, kv.second.worst);
  std::fprintf(stderr, "steps checked %ld, flagged %ld\n", total, flagged_total);
  return 0;
}
