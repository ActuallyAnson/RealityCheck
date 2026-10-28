// stepdump: links into the build under test (like mut_eval). For random scenarios it records, at sampled steps, the system's own state before and after one outer step,
// together with the scenario parameters, in the text format that refrun reads. It judges nothing.
//   stepdump --domain orbit|cartpole --scenarios N --base-seed S [--per-scenario 25]
#include <cmath>
#include <cstdio>
#include <iostream>

#include "sim/domains.hpp"

using namespace rc;

int main(int argc, char** argv) {
  std::string domain = "orbit";
  int n = 300, per = 25;
  uint64_t base = 700000;
  for (int i = 1; i + 1 < argc; i += 2) {
    std::string a = argv[i];
    if (a == "--domain") domain = argv[i + 1]; else if (a == "--scenarios") n = std::atoi(argv[i + 1]);
    else if (a == "--base-seed") base = static_cast<uint64_t>(std::atoll(argv[i + 1])); else if (a == "--per-scenario") per = std::atoi(argv[i + 1]);
  }
  for (int s = 0; s < n; ++s) {
    Scenario sc = random_scenario_for(domain, base + static_cast<uint64_t>(s));
    Trace tr = simulate(sc, 0);
    const int stride = std::max(1, sc.steps / per);
    for (int k = 0; k < sc.steps; k += stride) {
      const StepRecord &a = tr.rec[static_cast<size_t>(k)], &b = tr.rec[static_cast<size_t>(k) + 1];
      bool finite = true;
      for (const auto* r : {&a, &b}) for (const BodyState& q : r->body) finite &= std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.vx) && std::isfinite(q.vy);
      if (!finite) break;
      if (domain == "orbit") {
        std::printf("%d %d %.17g %.17g %zu", s, k, sc.dt, sc.gravity, sc.bodies.size());
        for (const auto& d : sc.bodies) std::printf(" %.17g", d.m);
        for (const auto* r : {&a, &b}) for (const BodyState& q : r->body) std::printf(" %.17g %.17g %.17g %.17g", q.x, q.y, q.vx, q.vy);
        std::printf("\n");
      } else if (domain == "cartpole") {
        const double t0 = k * sc.dt, t1 = (k + 1) * sc.dt;
        bool push = false;
        for (const auto& p : sc.cp.pushes) push |= p.t < t1 && p.t + p.len > t0;
        if (push) continue;  // an external push inside the step is not an input of the reference
        const CartPoleDef& c = sc.cp;
        std::printf("%d %d %.17g %.17g %.17g %.17g %.17g %.17g", s, k, sc.dt, c.mc, c.mp, c.l, c.g, b.fmag);
        for (const auto* r : {&a, &b}) std::printf(" %.17g %.17g %.17g %.17g", r->body[0].x, r->body[0].vx, r->body[0].y, r->body[0].vy);
        std::printf("\n");
      }
    }
  }
  return 0;
}
