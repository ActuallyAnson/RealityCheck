// Thin-wall tunneling test for Box2D fix commits (docs/REAL_PROJECTS.md). A circle fired at a thin static wall must never end on the far side of it; the
// truth needs no solver: a rigid body cannot pass through an impenetrable wall. Gravity 0, so a body that starts left of the wall and moves right can only
// end right of it by tunneling.
// ./thin_wall gen SEED N            one pipe-protocol rigid2d request per line (speed 20-120 m/s, wall thickness 0.02-0.2 m, radius 0.1-0.4, dt 1/60, 30 steps)
// ./thin_wall score REQ A.out B.out  scenarios in which A / B end beyond the wall; exact two-sided McNemar test on the discordant pairs
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

#include "util/json.hpp"

using rc::Json;

static double binom_two_sided(int a, int b) {
  const int n = a + b, m = std::min(a, b);
  double c = 1, s = 0;
  for (int k = 0; k <= m; ++k) { s += c; c = c * (n - k) / (k + 1); }
  return n ? std::min(1.0, 2 * s / std::pow(2.0, n)) : 1.0;
}

int main(int argc, char** argv) {
  const std::string mode = argc > 1 ? argv[1] : "";
  if (mode == "gen") {
    std::mt19937_64 rng(static_cast<unsigned>(std::atoi(argc > 2 ? argv[2] : "1")));
    const int n = argc > 3 ? std::atoi(argv[3]) : 1000;
    auto u = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
    for (int i = 0; i < n; ++i) {
      const double r = u(0.1, 0.4), th = u(0.02, 0.2), v = u(20, 120), y = u(2, 10);
      Json body = Json::object();
      body["x"] = 4.0; body["y"] = y; body["vx"] = v; body["vy"] = 0.0; body["r"] = r; body["m"] = 1.0; body["e"] = 0.0;
      Json bodies = Json::array(); bodies.push(body);
      Json wall = Json::object(); wall["x"] = 10.0; wall["y"] = 0.5; wall["w"] = th; wall["h"] = 11.0;
      Json walls = Json::array(); walls.push(wall);
      Json arena = Json::object(); arena["w"] = 20.0; arena["h"] = 12.0;
      Json sc = Json::object();
      sc["arena"] = arena; sc["bodies"] = bodies; sc["walls"] = walls; sc["dt"] = 1.0 / 60; sc["gravity"] = 0.0; sc["steps"] = 30; sc["seed"] = i;
      Json req = Json::object(); req["op"] = "simulate"; req["scenario"] = sc;
      std::cout << req.dump() << "\n";
    }
    return 0;
  }
  if (mode == "score" && argc > 4) {
    std::ifstream fr(argv[2]), fa(argv[3]), fb(argv[4]);
    std::string lr, la, lb;
    int n = 0, dropped = 0, ta = 0, tb = 0, only_a = 0, only_b = 0;
    while (std::getline(fr, lr) && std::getline(fa, la) && std::getline(fb, lb)) {
      const Json rq = Json::parse(lr), a = Json::parse(la), b = Json::parse(lb);
      if (!a.at("ok").boolean() || !b.at("ok").boolean()) { ++dropped; continue; }
      const Json& wall = rq.at("scenario").at("walls").arr().at(0);
      const double far_side = wall.num_or("x", 0) + wall.num_or("w", 0);
      auto beyond = [&](const Json& o) { const auto& fr2 = o.at("frames").arr(); return fr2.back().arr().at(0).num() > far_side; };
      const bool xa = beyond(a), xb = beyond(b);
      ++n; ta += xa; tb += xb; only_a += (xa && !xb); only_b += (!xa && xb);
    }
    std::printf("scenarios %d (dropped %d); beyond the wall at the end: A %d, B %d; only A %d, only B %d; two-sided exact McNemar p = %.2g\n", n, dropped, ta, tb, only_a, only_b, binom_two_sided(only_a, only_b));
    return 0;
  }
  return 2;
}
