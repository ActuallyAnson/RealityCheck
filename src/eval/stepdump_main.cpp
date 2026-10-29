// stepdump: links into the build under test (like mut_eval). For random scenarios it records, at sampled steps, the system's own state before and after one outer step,
// together with the scenario parameters, in the text format that refrun reads. It judges nothing.
//   stepdump --domain orbit|cartpole|traffic --scenarios N --base-seed S [--per-scenario 25]
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
    if (domain == "traffic") {
      // one line per second: parameters, which approaches can receive arrivals, the controller's phase and timer before the second (read off the recorded phases),
      // the events active this second, the recorded queues and waits before, then (for the harness only, never for the reference) the recorded phase, queues and waits after
      const TrafficDef& t = sc.tr;
      int mask = 0;
      for (int a = 0; a < 4; ++a) if (t.arrival[a] > 0) mask |= 1 << a;
      int began = 0;
      for (int k = 0; k < sc.steps; ++k) {
        const StepRecord &a = tr.rec[static_cast<size_t>(k)], &b = tr.rec[static_cast<size_t>(k) + 1];
        if (k > 0 && static_cast<int>(a.fmag) != static_cast<int>(tr.rec[static_cast<size_t>(k) - 1].fmag)) began = k;
        const int sec = k + 1;
        int em[4] = {0, 0, 0, 0}, fail[4] = {0, 0, 0, 0}, ped = 0;
        for (const auto& e : t.events) {
          if (e.kind == 0 && sec >= e.t && sec < e.t + e.duration) em[e.approach] = 1;
          if (e.kind == 2 && sec >= e.t && sec < e.t + e.duration) fail[e.approach] = 1;
          if (e.kind == 1 && e.t == sec) ped = 1;
        }
        std::printf("%d %d %d %d %d %d %d %d %d %d %d %d", s, k, t.green_ns, t.green_ew, t.yellow, t.all_red, t.max_green, t.actuated ? 1 : 0, mask, static_cast<int>(a.fmag), k - began, sec);
        for (int i = 0; i < 4; ++i) std::printf(" %d", static_cast<int>(a.body[static_cast<size_t>(i)].y));
        for (int i = 0; i < 4; ++i) std::printf(" %d", static_cast<int>(a.body[static_cast<size_t>(i)].vx));
        for (int i = 0; i < 4; ++i) std::printf(" %d", em[i]);
        for (int i = 0; i < 4; ++i) std::printf(" %d", fail[i]);
        std::printf(" %d %d", ped, static_cast<int>(b.fmag));
        for (int i = 0; i < 4; ++i) std::printf(" %d", static_cast<int>(b.body[static_cast<size_t>(i)].y));
        for (int i = 0; i < 4; ++i) std::printf(" %d", static_cast<int>(b.body[static_cast<size_t>(i)].vx));
        std::printf("\n");
      }
      continue;
    }
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
