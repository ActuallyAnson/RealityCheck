#include "shrink/shrink.hpp"

#include <cmath>
#include <functional>

#include "sim/world.hpp"

namespace rc {
namespace {

struct Shrinker {
  FailFn fails;  // true when the candidate still fails; fills the violation
  int max_attempts;
  int attempts = 0, accepted = 0;
  long long steps_simulated = 0;
  Scenario cur;
  Violation cur_v;

  // Runs the candidate; true when it is valid and still violates `rule`.
  bool reproduces(const Scenario& c, Violation* out) {
    if (attempts >= max_attempts) return false;
    if (!validate(c).empty()) return false;
    ++attempts;
    steps_simulated += c.steps;
    Violation v;
    if (!fails(c, &v)) return false;
    if (out) *out = v;
    return true;
  }

  bool accept(Scenario c) {
    Violation v;
    if (!reproduces(c, &v)) return false;
    cur = std::move(c);
    cur_v = v;
    ++accepted;
    truncate();
    return true;
  }

  void truncate() {
    int want = std::max(1, cur_v.step);
    if (want < cur.steps) {
      Scenario c = cur;
      c.steps = want;
      Violation v;
      if (reproduces(c, &v)) { cur = c; cur_v = v; }
    }
  }

  bool try_field(const std::function<void(Scenario&)>& edit) {
    Scenario c = cur;
    edit(c);
    if (c == cur) return false;
    return accept(c);
  }
};

}  // namespace

ShrinkResult shrink(const Scenario& sc, const SimFn& sim, const std::string& rule, int max_attempts) {
  return shrink(sc, [&sim, &rule](const Scenario& c, Violation* out) {
    Trace tr = sim(c);
    for (const Violation& v : check_rules(c, tr)) {
      if (v.rule == rule) {
        if (out) *out = v;
        return true;
      }
    }
    return false;
  }, max_attempts);
}

ShrinkResult shrink(const Scenario& sc, const FailFn& fails, int max_attempts) {
  Shrinker s;
  s.fails = fails;
  s.max_attempts = max_attempts;
  ShrinkResult res;
  res.size_before = scenario_size(sc);
  if (!validate(sc).empty()) return res;
  Violation v0;
  s.max_attempts += 1;
  if (!s.reproduces(sc, &v0)) return res;
  s.cur = sc;
  s.cur_v = v0;
  s.truncate();

  bool progress = true;
  while (progress && s.attempts < s.max_attempts) {
    progress = false;
    // structure: walls, controller, bodies
    for (size_t i = s.cur.walls.size(); i-- > 0;)
      progress |= s.try_field([&](Scenario& c) { c.walls.erase(c.walls.begin() + static_cast<long>(i)); });
    if (s.cur.ctrl.enabled) {
      progress |= s.try_field([](Scenario& c) { c.ctrl.enabled = false; });
      for (size_t i = s.cur.ctrl.waypoints.size(); i-- > 0 && s.cur.ctrl.waypoints.size() > 1;)
        progress |= s.try_field([&](Scenario& c) {
          if (c.ctrl.waypoints.size() > 1) c.ctrl.waypoints.erase(c.ctrl.waypoints.begin() + static_cast<long>(i));
        });
    }
    for (size_t i = s.cur.bodies.size(); i-- > 0;) {
      if (s.cur.bodies.size() <= 1) break;
      progress |= s.try_field([&](Scenario& c) {
        c.bodies.erase(c.bodies.begin() + static_cast<long>(i));
        if (c.ctrl.enabled) {
          if (c.ctrl.body == static_cast<int>(i)) c.ctrl.enabled = false;
          else if (c.ctrl.body > static_cast<int>(i)) --c.ctrl.body;
        }
      });
    }
    if (s.cur.domain == "cartpole") {
      for (size_t i = s.cur.cp.pushes.size(); i-- > 0;)
        progress |= s.try_field([&](Scenario& c) { c.cp.pushes.erase(c.cp.pushes.begin() + static_cast<long>(i)); });
      for (size_t i = 0; i < s.cur.cp.pushes.size(); ++i) {
        progress |= s.try_field([i](Scenario& c) { c.cp.pushes[i].len = std::max(c.dt, 0.1); });
        progress |= s.try_field([i](Scenario& c) { c.cp.pushes[i].force = std::round(c.cp.pushes[i].force); });
        progress |= s.try_field([i](Scenario& c) { c.cp.pushes[i].t = std::round(c.cp.pushes[i].t * 10) / 10; });
      }
      progress |= s.try_field([](Scenario& c) { c.cp.noise = 0; });
      progress |= s.try_field([](Scenario& c) { c.seed = 1; });
      progress |= s.try_field([](Scenario& c) { c.cp.x0 = 0; });
      progress |= s.try_field([](Scenario& c) { c.cp.v0 = 0; });
      progress |= s.try_field([](Scenario& c) { c.cp.w0 = 0; });
      progress |= s.try_field([](Scenario& c) { c.cp.g = 9.81; });
      progress |= s.try_field([](Scenario& c) { c.cp.mc = 1.0; });
      progress |= s.try_field([](Scenario& c) { c.cp.mp = 0.2; });
      progress |= s.try_field([](Scenario& c) { c.cp.l = 0.5; });
      progress |= s.try_field([](Scenario& c) { c.cp.fmax = 30; });
      progress |= s.try_field([](Scenario& c) { c.cp.th0 = std::round(c.cp.th0 * 20) / 20; });
    }
    // simplify scalars
    progress |= s.try_field([](Scenario& c) { c.ctrl.noise = 0; });
    progress |= s.try_field([](Scenario& c) { c.seed = 1; });
    progress |= s.try_field([](Scenario& c) { c.gravity = 9.81; });
    for (size_t i = 0; i < s.cur.bodies.size(); ++i) {
      for (int comp = 0; comp < 2; ++comp) {
        auto set = [comp, i](Scenario& c, double f) {
          double& v = comp == 0 ? c.bodies[i].vx : c.bodies[i].vy;
          v *= f;
        };
        if (s.try_field([&](Scenario& c) { set(c, 0.0); })) { progress = true; continue; }
        for (int k = 0; k < 6; ++k) {
          if (s.try_field([&](Scenario& c) { set(c, 0.5); })) progress = true; else break;
        }
      }
      progress |= s.try_field([i](Scenario& c) { c.bodies[i].m = 1.0; });
      progress |= s.try_field([i](Scenario& c) { c.bodies[i].r = 0.5; });
      progress |= s.try_field([i](Scenario& c) { c.bodies[i].e = std::round(c.bodies[i].e * 10) / 10; });
      progress |= s.try_field([i](Scenario& c) {
        c.bodies[i].x = std::round(c.bodies[i].x * 10) / 10;
        c.bodies[i].y = std::round(c.bodies[i].y * 10) / 10;
      });
    }
    for (size_t i = 0; i < s.cur.walls.size(); ++i)
      progress |= s.try_field([i](Scenario& c) {
        WallDef& w = c.walls[i];
        w.x = std::round(w.x * 10) / 10; w.y = std::round(w.y * 10) / 10;
        w.w = std::round(w.w * 10) / 10; w.h = std::round(w.h * 10) / 10;
      });
    for (size_t i = 0; i < s.cur.ctrl.waypoints.size() && s.cur.ctrl.enabled; ++i)
      progress |= s.try_field([i](Scenario& c) {
        c.ctrl.waypoints[i].x = std::round(c.ctrl.waypoints[i].x * 10) / 10;
        c.ctrl.waypoints[i].y = std::round(c.ctrl.waypoints[i].y * 10) / 10;
      });
    // fewer steps than the first violation can happen after other simplifications
    progress |= s.try_field([](Scenario& c) { c.steps = std::max(1, c.steps / 2); });
  }

  // Final guard: the returned scenario must reproduce the failure.
  s.max_attempts = s.attempts + 2;
  Violation vf;
  if (!s.reproduces(s.cur, &vf)) return res;
  res.ok = true;
  res.scenario = s.cur;
  res.violation = vf;
  res.attempts = s.attempts;
  res.accepted = s.accepted;
  res.steps_simulated = s.steps_simulated;
  res.size_after = scenario_size(s.cur);
  return res;
}

}  // namespace rc
