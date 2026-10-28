#include "rules/checker.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <utility>

#include "sim/orbit.hpp"

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
  if (domain == "orbit") return orbit_cat;
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
  if (sc.domain == "orbit") return check_orbit_rules(sc, tr);
  if (sc.domain == "cartpole") return check_cartpole_rules(sc, tr);
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
