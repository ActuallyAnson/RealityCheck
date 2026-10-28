#include "baseline/random_baseline.hpp"

#include <cmath>

#include "sim/cartpole.hpp"
#include "sim/traffic.hpp"
#include "sim/orbit.hpp"
#include "util/rng.hpp"

namespace rc {

Scenario random_scenario(uint64_t seed) {
  using L = ScenarioLimits;
  Rng rng(seed);
  for (;;) {
    Scenario s;
    s.seed = rng.next() % 1000000;
    s.dt = rng.range(L::dt_min, L::dt_max);
    s.steps = rng.irange(100, L::steps_max);
    s.gravity = rng.uni() < 0.8 ? 9.81 : rng.range(0, 20);
    int nb = rng.irange(1, L::bodies_max);
    int nw = rng.irange(0, L::walls_max);
    for (int i = 0; i < nw; ++i) {
      WallDef w;
      bool horizontal = rng.uni() < 0.5;
      double len = rng.range(2, 8), thick = rng.range(L::wall_thickness_min, 0.6);
      w.w = horizontal ? len : thick;
      w.h = horizontal ? thick : len;
      w.x = rng.range(0, s.arena_w - w.w);
      w.y = rng.range(0, s.arena_h - w.h);
      s.walls.push_back(w);
    }
    for (int i = 0; i < nb; ++i) {
      BodyDef b;
      b.r = rng.range(L::r_min, L::r_max);
      b.m = rng.range(L::m_min, L::m_max);
      b.e = rng.range(0, 1);
      b.x = rng.range(b.r, s.arena_w - b.r);
      b.y = rng.range(b.r, s.arena_h - b.r);
      double sp = rng.range(0, L::speed_max), an = rng.range(0, 6.283185307179586);
      b.vx = sp * std::cos(an);
      b.vy = sp * std::sin(an);
      s.bodies.push_back(b);
    }
    if (rng.uni() < 0.5) {
      s.ctrl.enabled = true;
      s.ctrl.body = rng.irange(0, nb - 1);
      s.ctrl.kp = rng.range(1, 30);
      s.ctrl.fmax = rng.range(L::fmax_min, L::fmax_max);
      s.ctrl.vcmd = rng.range(1, 15);
      s.ctrl.noise = rng.uni() < 0.5 ? 0.0 : rng.range(0, 0.3);
      int nw2 = rng.irange(1, L::waypoints_max);
      for (int i = 0; i < nw2; ++i) s.ctrl.waypoints.push_back({rng.range(0, s.arena_w), rng.range(0, s.arena_h)});
    }
    if (validate(s).empty()) return s;  // rejection sampling: overlapping layouts are redrawn
  }
}

HuntResult random_baseline(const SimFn& sim, uint64_t seed, const Budget& budget) {
  HuntResult res;
  Rng rng(seed);
  auto t0 = std::chrono::steady_clock::now();
  auto elapsed = [&] {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  };
  while (res.steps_used < budget.max_steps && elapsed() < budget.max_wall_ms) {
    Scenario s = budget.domain == "orbit"      ? random_orbit_scenario(rng.next())
                 : budget.domain == "cartpole" ? random_cartpole_scenario(rng.next())
                 : budget.domain == "traffic"  ? random_traffic_scenario(rng.next())
                                               : random_scenario(rng.next());
    long long left = budget.max_steps - res.steps_used;
    if (s.steps > left) s.steps = static_cast<int>(left);  // honour the step budget exactly
    Trace tr = sim(s);
    res.steps_used += s.steps;
    ++res.scenarios_run;
    auto v = check_rules(s, tr);
    if (!v.empty()) {
      res.found = true;
      res.scenarios_until_first = res.scenarios_run;
      res.first_failure = s;
      res.first_violation = v.front();
      break;
    }
  }
  res.wall_ms = elapsed();
  return res;
}

}  // namespace rc
