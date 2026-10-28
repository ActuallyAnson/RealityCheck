#include "rules/checker.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <utility>

#include "sim/orbit.hpp"
#include "sim/generic.hpp"
#include "sim/traffic.hpp"

namespace rc {

const std::vector<RuleInfo>& rule_catalog(const std::string& domain) {
  static const std::vector<RuleInfo> orbit_cat = {
      {"no_nan", "Every position, velocity and energy value stays finite."},
      {"energy_conserved", "Total energy (kinetic plus softened potential) stays within 2% of the largest kinetic-plus-|potential| seen, plus 0.05."},
      {"momentum_conserved", "Total linear momentum stays within 0.1% of the largest sum of m|v| seen, plus 0.001."},
      {"angular_momentum_conserved", "Total angular momentum about the origin stays within 0.1% of the largest sum of m|r||v| seen, plus 0.001."},
  };
  static const std::vector<RuleInfo> cart_cat = {
      {"no_nan", "Every state value and the controller force stay finite."},
      {"pole_upright", "The pole angle from upright stays within 0.8 rad at every step."},
      {"cart_in_track", "The cart position stays within the 2.4 m track (|x| <= 2.4)."},
      {"controller_force_limit", "The controller never applies a force above the scenario's force_limit."},
      {"energy_bound", "Mechanical energy never exceeds the initial energy plus the work of all external forces, beyond 5% of (|E0|+|work|) plus 0.05."},
  };
  static const std::vector<RuleInfo> traffic_cat = {
      {"valid_signal", "Every approach shows a valid signal (red, yellow or green) and a non-negative, finite queue."},
      {"no_conflicting_signals", "The N/S and E/W directions are never both non-red (green or yellow) in the same second."},
      {"all_red_clearance", "Before a direction turns green, the opposite direction has been red for at least all_red_seconds."},
      {"yellow_duration", "A green always ends in yellow, and every yellow lasts at least yellow_seconds before red."},
      {"max_wait", "No approach waits at red with vehicles queued for more than 200 seconds."},
  };
  if (domain == "generic") return generic_rule_catalog();
  if (domain == "orbit") return orbit_cat;
  if (domain == "traffic") return traffic_cat;
  if (domain == "cartpole") return cart_cat;
  static const std::vector<RuleInfo> cat = {
      {"no_nan", "Every position, velocity and energy value stays finite."},
      {"in_arena", "Every body stays inside the 20 x 12 arena (centre +/- radius within the bounds, tolerance 0.15)."},
      {"no_wall_penetration", "No body overlaps a wall box by more than 0.15."},
      {"no_tunneling", "No body centre ever crosses through a wall box between consecutive steps."},
      {"energy_bound", "Total mechanical energy never exceeds initial energy plus controller work, beyond a 10% + 5 J tolerance."},
      {"controller_force_limit", "The controller never commands a force above its declared fmax."},
  };
  return cat;
}

namespace {
std::string fmt(const char* f, double a = 0, double b = 0, double c = 0);
}

std::vector<Violation> check_traffic_rules(const Scenario& sc, const Trace& tr) {
  std::vector<Violation> out;
  std::set<std::pair<std::string, int>> seen;
  auto add = [&](Violation v) {
    if (seen.insert({v.rule, v.body}).second) out.push_back(std::move(v));
  };
  const int n = tr.steps();
  auto sig = [&](int k, int a) { return static_cast<int>(tr.rec[static_cast<size_t>(k)].body[static_cast<size_t>(a)].x); };
  std::vector<int> yellow_run(4, 0);
  for (int k = 0; k <= n; ++k) {
    const StepRecord& r = tr.rec[static_cast<size_t>(k)];
    for (int a = 0; a < 4; ++a) {
      const BodyState& q = r.body[static_cast<size_t>(a)];
      if (!(std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.vx)) || q.x < 0 || q.x > 2 || q.y < 0)
        add({"valid_signal", k, a, q.x, 0, "invalid signal or queue value"});
      if (q.vx > TrafficLimits::wait_limit)
        add({"max_wait", k, a, q.vx, TrafficLimits::wait_limit, fmt("approach %.0f has waited %.0f s at red with vehicles queued (limit 200)", a, q.vx)});
    }
    bool ns = sig(k, 0) != 0 || sig(k, 2) != 0, ew = sig(k, 1) != 0 || sig(k, 3) != 0;
    if (ns && ew) add({"no_conflicting_signals", k, -1, 0, 0, "N/S and E/W are both non-red"});
    for (int a = 0; a < 4; ++a) {
      int cur = sig(k, a), prev = k > 0 ? sig(k - 1, a) : cur;
      if (cur == 2 && prev != 2) {  // a green starts: the other direction must have been red for all_red seconds
        for (int back = 1; back <= sc.tr.all_red && k - back >= 0; ++back)
          for (int o = 1 - a % 2; o < 4; o += 2)
            if (sig(k - back, o) != 0) {
              add({"all_red_clearance", k, a, static_cast<double>(back - 1), static_cast<double>(sc.tr.all_red),
                   fmt("approach %.0f turned green after only %.0f s of all-red (need %.0f s)", a, back - 1.0, sc.tr.all_red)});
              back = sc.tr.all_red + 1;
              break;
            }
      }
      if (cur == 1) ++yellow_run[static_cast<size_t>(a)];
      if (prev == 2 && cur == 0) add({"yellow_duration", k, a, 0, static_cast<double>(sc.tr.yellow), fmt("approach %.0f went from green straight to red (no yellow)", a)});
      if (prev == 1 && cur != 1) {
        if (cur == 0 && yellow_run[static_cast<size_t>(a)] < sc.tr.yellow)
          add({"yellow_duration", k, a, static_cast<double>(yellow_run[static_cast<size_t>(a)]), static_cast<double>(sc.tr.yellow), fmt("yellow lasted %.0f s on approach %.0f", yellow_run[static_cast<size_t>(a)], a)});
        yellow_run[static_cast<size_t>(a)] = 0;
      }
    }
  }
  std::stable_sort(out.begin(), out.end(), [](const Violation& a, const Violation& b) {
    if (a.step != b.step) return a.step < b.step;
    return a.rule < b.rule;
  });
  return out;
}

std::vector<Violation> check_cartpole_rules(const Scenario& sc, const Trace& tr) {
  std::vector<Violation> out;
  std::set<std::string> seen;
  auto add = [&](Violation v) {
    if (seen.insert(v.rule).second) out.push_back(std::move(v));
  };
  if (tr.rec.empty()) return out;
  const double e0 = tr.rec[0].energy;
  for (int k = 0; k <= tr.steps(); ++k) {
    const StepRecord& s = tr.rec[static_cast<size_t>(k)];
    const BodyState& q = s.body[0];
    if (!(std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.vx) && std::isfinite(q.vy) && std::isfinite(s.energy) &&
          std::isfinite(s.fmag) && std::isfinite(s.work))) {
      add({"no_nan", k, -1, 0, 0, "non-finite state or force"});
      break;
    }
    if (std::fabs(q.y) > 0.8)
      add({"pole_upright", k, 0, std::fabs(q.y), 0.8, fmt("pole angle %.3f rad from upright (limit 0.8)", q.y)});
    if (std::fabs(q.x) > 2.4)
      add({"cart_in_track", k, 0, std::fabs(q.x), 2.4, fmt("cart at x=%.3f outside the +/-2.4 track", q.x)});
    if (k > 0 && s.fmag > sc.cp.fmax * (1 + 1e-6))
      add({"controller_force_limit", k, 0, s.fmag, sc.cp.fmax, fmt("controller force %.3f exceeds limit %.3f", s.fmag, sc.cp.fmax)});
    double lim = e0 + s.work + 0.05 * (std::fabs(e0) + std::fabs(s.work)) + 0.05;
    if (s.energy > lim)
      add({"energy_bound", k, -1, s.energy, lim, fmt("energy %.3f exceeds allowed %.3f", s.energy, lim)});
  }
  std::stable_sort(out.begin(), out.end(), [](const Violation& a, const Violation& b) {
    if (a.step != b.step) return a.step < b.step;
    return a.rule < b.rule;
  });
  return out;
}

std::vector<Violation> check_orbit_rules(const Scenario& sc, const Trace& tr) {
  std::vector<Violation> out;
  std::set<std::string> seen;
  auto add = [&](Violation v) {
    if (seen.insert(v.rule).second) out.push_back(std::move(v));
  };
  if (tr.rec.empty()) return out;
  double escale = 0, pscale = 0, lscale = 0;
  bool finite_all = true;
  for (const StepRecord& s : tr.rec) {
    double ke, pe;
    orbit_energies(sc, s, &ke, &pe);
    escale = std::max(escale, ke + std::fabs(pe));
    double ps = 0, ls = 0;
    for (size_t i = 0; i < s.body.size(); ++i) {
      const BodyState& q = s.body[i];
      double m = sc.bodies[i].m;
      ps += m * std::hypot(q.vx, q.vy);
      ls += m * std::hypot(q.x, q.y) * std::hypot(q.vx, q.vy);
    }
    if (std::isfinite(ps)) pscale = std::max(pscale, ps);
    if (std::isfinite(ls)) lscale = std::max(lscale, ls);
  }
  const StepRecord& r0 = tr.rec[0];
  for (int k = 0; k <= tr.steps(); ++k) {
    const StepRecord& s = tr.rec[static_cast<size_t>(k)];
    bool fin = std::isfinite(s.energy) && std::isfinite(s.px) && std::isfinite(s.py) && std::isfinite(s.ang);
    for (const auto& q : s.body) fin = fin && std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.vx) && std::isfinite(q.vy);
    if (!fin) { add({"no_nan", k, -1, 0, 0, "non-finite state"}); finite_all = false; break; }
    double elim = 0.02 * escale + 0.05;
    if (std::fabs(s.energy - r0.energy) > elim)
      add({"energy_conserved", k, -1, std::fabs(s.energy - r0.energy), elim,
           fmt("energy changed by %.4f (allowed %.4f, E0 %.4f)", std::fabs(s.energy - r0.energy), elim, r0.energy)});
    double dp = std::hypot(s.px - r0.px, s.py - r0.py), plim = 1e-3 * pscale + 1e-3;
    if (dp > plim)
      add({"momentum_conserved", k, -1, dp, plim, fmt("total momentum changed by %.5f (allowed %.5f)", dp, plim)});
    double dl = std::fabs(s.ang - r0.ang), llim = 1e-3 * lscale + 1e-3;
    if (dl > llim)
      add({"angular_momentum_conserved", k, -1, dl, llim, fmt("angular momentum changed by %.5f (allowed %.5f)", dl, llim)});
  }
  (void)finite_all;
  std::stable_sort(out.begin(), out.end(), [](const Violation& a, const Violation& b) {
    if (a.step != b.step) return a.step < b.step;
    return a.rule < b.rule;
  });
  return out;
}

namespace {

bool segment_hits_box(double x0, double y0, double x1, double y1, const WallDef& w) {
  double t0 = 0, t1 = 1;
  double dx = x1 - x0, dy = y1 - y0;
  const double lo[2] = {w.x, w.y}, hi[2] = {w.x + w.w, w.y + w.h};
  const double o[2] = {x0, y0}, d[2] = {dx, dy};
  for (int a = 0; a < 2; ++a) {
    if (std::fabs(d[a]) < 1e-15) {
      if (o[a] <= lo[a] || o[a] >= hi[a]) return false;
    } else {
      double ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a];
      if (ta > tb) std::swap(ta, tb);
      t0 = std::fmax(t0, ta);
      t1 = std::fmin(t1, tb);
      if (t0 >= t1) return false;
    }
  }
  return true;
}

std::string fmt(const char* f, double a, double b, double c) {
  char buf[200];
  std::snprintf(buf, sizeof buf, f, a, b, c);
  return buf;
}

}  // namespace

std::vector<Violation> check_rules(const Scenario& sc, const Trace& tr, const RuleSpec& spec) {
  if (sc.domain == "generic") return check_generic_rules(sc, tr);
  if (sc.domain == "orbit") return check_orbit_rules(sc, tr);
  if (sc.domain == "cartpole") return check_cartpole_rules(sc, tr);
  if (sc.domain == "traffic") return check_traffic_rules(sc, tr);
  std::vector<Violation> out;
  std::set<std::pair<std::string, int>> seen;
  auto add = [&](Violation v) {
    if (seen.insert({v.rule, v.body}).second) out.push_back(std::move(v));
  };
  const double e0 = tr.rec.empty() ? 0 : tr.rec[0].energy;

  for (int k = 0; k <= tr.steps(); ++k) {
    const StepRecord& s = tr.rec[static_cast<size_t>(k)];
    bool finite = std::isfinite(s.energy) && std::isfinite(s.work) && std::isfinite(s.fmag);
    for (size_t i = 0; i < s.body.size(); ++i) {
      const BodyState& q = s.body[i];
      const BodyDef& d = sc.bodies[i];
      int bi = static_cast<int>(i);
      if (!(std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.vx) && std::isfinite(q.vy))) {
        add({"no_nan", k, bi, 0, 0, "non-finite position or velocity"});
        finite = false;
        continue;
      }
      double over = std::fmax(std::fmax(d.r - q.x, q.x + d.r - sc.arena_w), std::fmax(d.r - q.y, q.y + d.r - sc.arena_h));
      if (over > spec.pen_tol)
        add({"in_arena", k, bi, over, spec.pen_tol, fmt("body extends %.4f beyond the arena (limit %.4f)", over, spec.pen_tol)});
      for (size_t wi = 0; wi < sc.walls.size(); ++wi) {
        const WallDef& w = sc.walls[wi];
        double cx = std::fmax(w.x, std::fmin(q.x, w.x + w.w));
        double cy = std::fmax(w.y, std::fmin(q.y, w.y + w.h));
        double dist = std::hypot(q.x - cx, q.y - cy);
        double pen = d.r - dist;
        if (pen > spec.pen_tol)
          add({"no_wall_penetration", k, bi, pen, spec.pen_tol,
               fmt("overlaps wall %.0f by %.4f (limit %.4f)", static_cast<double>(wi), pen, spec.pen_tol)});
        if (k > 0) {
          const BodyState& p = tr.rec[static_cast<size_t>(k - 1)].body[i];
          if (std::isfinite(p.x) && std::isfinite(p.y) && segment_hits_box(p.x, p.y, q.x, q.y, w))
            add({"no_tunneling", k, bi, 0, 0,
                 fmt("body centre moved through wall %.0f between steps %.0f and %.0f", static_cast<double>(wi),
                     static_cast<double>(k - 1), static_cast<double>(k))});
        }
      }
    }
    if (!finite) {
      add({"no_nan", k, -1, 0, 0, "non-finite energy, work or force"});
      continue;
    }
    double scale = std::fabs(e0) + std::fabs(s.work);
    double limit = e0 + s.work + spec.energy_rel * scale + spec.energy_abs;
    if (s.energy > limit)
      add({"energy_bound", k, -1, s.energy, limit,
           fmt("energy %.3f exceeds allowed %.3f (E0 %.3f)", s.energy, limit, e0)});
    if (sc.ctrl.enabled && k > 0 && s.fmag > sc.ctrl.fmax * (1 + spec.force_tol))
      add({"controller_force_limit", k, sc.ctrl.body, s.fmag, sc.ctrl.fmax,
           fmt("controller force %.3f exceeds limit %.3f", s.fmag, sc.ctrl.fmax)});
  }
  std::stable_sort(out.begin(), out.end(), [](const Violation& a, const Violation& b) {
    if (a.step != b.step) return a.step < b.step;
    return a.rule < b.rule;
  });
  return out;
}

std::map<std::string, double> rule_margins(const Scenario& sc, const Trace& tr) {
  std::map<std::string, double> m;
  auto up = [&](const char* rule, double v) {
    if (!std::isfinite(v)) v = 1e9;
    auto it = m.find(rule);
    if (it == m.end() || v > it->second) m[rule] = v;
  };
  if (tr.rec.empty() || sc.domain == "generic") return m;
  const StepRecord& r0 = tr.rec[0];
  if (sc.domain == "traffic") {
    for (const StepRecord& r : tr.rec)
      for (const BodyState& q : r.body) up("max_wait", q.vx / TrafficLimits::wait_limit);
  } else if (sc.domain == "cartpole") {
    for (size_t k = 0; k < tr.rec.size(); ++k) {
      const StepRecord& s = tr.rec[k];
      const BodyState& q = s.body[0];
      up("pole_upright", std::fabs(q.y) / 0.8);
      up("cart_in_track", std::fabs(q.x) / 2.4);
      if (k > 0) up("controller_force_limit", s.fmag / (sc.cp.fmax * (1 + 1e-6)));
      double base = r0.energy + s.work, slack = 0.05 * (std::fabs(r0.energy) + std::fabs(s.work)) + 0.05;
      up("energy_bound", (s.energy - base) / slack);
    }
  } else if (sc.domain == "orbit") {
    double escale = 0, pscale = 0, lscale = 0;
    for (const StepRecord& s : tr.rec) {
      double ke, pe, ps = 0, ls = 0;
      orbit_energies(sc, s, &ke, &pe);
      escale = std::max(escale, ke + std::fabs(pe));
      for (size_t i = 0; i < s.body.size(); ++i) {
        ps += sc.bodies[i].m * std::hypot(s.body[i].vx, s.body[i].vy);
        ls += sc.bodies[i].m * std::hypot(s.body[i].x, s.body[i].y) * std::hypot(s.body[i].vx, s.body[i].vy);
      }
      if (std::isfinite(ps)) pscale = std::max(pscale, ps);
      if (std::isfinite(ls)) lscale = std::max(lscale, ls);
    }
    for (const StepRecord& s : tr.rec) {
      up("energy_conserved", std::fabs(s.energy - r0.energy) / (0.02 * escale + 0.05));
      up("momentum_conserved", std::hypot(s.px - r0.px, s.py - r0.py) / (1e-3 * pscale + 1e-3));
      up("angular_momentum_conserved", std::fabs(s.ang - r0.ang) / (1e-3 * lscale + 1e-3));
    }
  } else {
    RuleSpec spec;
    for (const StepRecord& s : tr.rec) {
      for (size_t i = 0; i < s.body.size(); ++i) {
        const BodyState& q = s.body[i];
        const BodyDef& d = sc.bodies[i];
        double over = std::fmax(std::fmax(d.r - q.x, q.x + d.r - sc.arena_w), std::fmax(d.r - q.y, q.y + d.r - sc.arena_h));
        up("in_arena", over / spec.pen_tol);
        for (const WallDef& w : sc.walls) {
          double cx = std::fmax(w.x, std::fmin(q.x, w.x + w.w)), cy = std::fmax(w.y, std::fmin(q.y, w.y + w.h));
          up("no_wall_penetration", (d.r - std::hypot(q.x - cx, q.y - cy)) / spec.pen_tol);
        }
      }
      double base = r0.energy + s.work, slack = spec.energy_rel * (std::fabs(r0.energy) + std::fabs(s.work)) + spec.energy_abs;
      up("energy_bound", (s.energy - base) / slack);
      if (sc.ctrl.enabled) up("controller_force_limit", s.fmag / (sc.ctrl.fmax * (1 + spec.force_tol)));
    }
  }
  return m;
}

Json to_json(const Violation& v) {
  Json j = Json::object();
  j["rule"] = v.rule;
  j["step"] = v.step;
  j["body"] = v.body;
  j["value"] = v.value;
  j["limit"] = v.limit;
  j["detail"] = v.detail;
  return j;
}

}  // namespace rc
