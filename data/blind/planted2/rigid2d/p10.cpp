#include "sim/world.hpp"
#include "sim/orbit.hpp"
#include "sim/cartpole.hpp"
#include "sim/traffic.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "util/rng.hpp"

namespace rc {
namespace {

struct Body {
  double x, y, vx, vy, r, m, e, inv_m, gscale;
};

constexpr double kRestSpeed = 0.5;     // below this approach speed contacts are inelastic
constexpr double kSlop = 0.001;
constexpr double kCorrect = 0.8;       // positional correction fraction
constexpr double kMaxMove = 0.03;      // target travel per substep, keeps thin walls from being skipped
constexpr int kMaxSubsteps = 1000;     // per outer step; substep length is re-derived from the CURRENT speed each time

struct Sim {
  const Scenario& sc;
  BugMask bugs;
  std::vector<Body> b;
  double work = 0;
  double fmag_step = 0;
  int wp = 0;
  double noise_x = 0, noise_y = 0;
  Rng rng;

  Sim(const Scenario& s, BugMask mask) : sc(s), bugs(mask), rng(s.seed ^ 0xA5A5A5A5DEADBEEFULL) {
    for (size_t i = 0; i < s.bodies.size(); ++i) {
      const BodyDef& d = s.bodies[i];
      Body body{d.x, d.y, d.vx, d.vy, d.r, d.m, d.e, 1.0 / d.m, 1.0};
      if (s.ctrl.enabled && static_cast<int>(i) == s.ctrl.body) body.gscale = 0.0;
      b.push_back(body);
    }
  }

  bool has(Bug k) const { return (bugs & bug_bit(k)) != 0; }

  double energy() const {
    double e = 0;
    for (const Body& q : b) e += 0.5 * q.m * (q.vx * q.vx + q.vy * q.vy) + q.m * sc.gravity * q.gscale * q.y;
    return e;
  }

  StepRecord record(int substeps, double fmag) const {
    StepRecord r;
    for (const Body& q : b) r.body.push_back({q.x, q.y, q.vx, q.vy});
    r.energy = energy();
    r.work = work;
    r.fmag = fmag;
    r.substeps = substeps;
    r.waypoint = wp;
    return r;
  }

  double restitution(double e) const {
    if (has(kRestitutionGain) && e > 0.9) return e + 0.15;
    return e;
  }

  // Extra separating velocity applied by a faulty penetration-recovery term (b03).
  double recovery_kick(double pen) const {
    if (!has(kSmallDtBlowup)) return 0.0;
    return pen * 1e-3 / (sc.dt * sc.dt);
  }

  void control(Body& q, double& fx, double& fy) {
    fx = fy = 0;
    const ControllerDef& c = sc.ctrl;
    const Waypoint& t0 = c.waypoints[static_cast<size_t>(wp)];
    double ex = t0.x - (q.x + noise_x), ey = t0.y - (q.y + noise_y);
    double dist = std::sqrt(ex * ex + ey * ey);
    if (dist < 0.3 && wp + 1 < static_cast<int>(c.waypoints.size())) {
      ++wp;
      const Waypoint& t1 = c.waypoints[static_cast<size_t>(wp)];
      ex = t1.x - (q.x + noise_x);
      ey = t1.y - (q.y + noise_y);
      dist = std::sqrt(ex * ex + ey * ey);
    }
    double dx, dy;
    if (has(kZeroDirNaN)) {          // b09: no guard for a zero-length direction
      dx = ex / dist; dy = ey / dist;
    } else if (dist > 1e-9) {
      dx = ex / dist; dy = ey / dist;
    } else {
      dx = dy = 0;
    }
    double speed = std::fmin(c.vcmd, 2.0 * dist);
    fx = c.kp * (dx * speed - q.vx);
    fy = c.kp * (dy * speed - q.vy);
    double f = std::sqrt(fx * fx + fy * fy);
    bool clamp = !(has(kClampSkipNear) && dist < 1.5);  // b08: clamp skipped near the target
    double fcap = (has(kNoisyClampLoose) && c.noise > 0.2 && c.kp > 25) ? 1.3 * c.fmax : c.fmax;  // b13: loose clamp for a noisy, stiff controller
    if (clamp && f > fcap) {
      fx *= fcap / f;
      fy *= fcap / f;
      f = fcap;
    }
    fmag_step = std::fmax(fmag_step, f);
  }

  void collide_bodies() {
    for (size_t i = 0; i < b.size(); ++i) {
      for (size_t j = i + 1; j < b.size(); ++j) {
        Body& a = b[i];
        Body& c = b[j];
        double dx = c.x - a.x, dy = c.y - a.y;
        double d = std::sqrt(dx * dx + dy * dy);
        double rr = a.r + c.r;
        if (has(kRestingKick) && d < rr + 0.01 && d > 1e-12 && std::fabs(dy) > 0.7 * d) {
          // b06: "resting contact stabilizer" adds a separating velocity to touching vertical contacts
          double kx = dx / d, ky = dy / d;
          a.vx -= 0.5 * 0.8 * kx; a.vy -= 0.5 * 0.8 * ky;
          c.vx += 0.5 * 0.8 * kx; c.vy += 0.5 * 0.8 * ky;
        }
        if (d >= rr) continue;
        double nx, ny;
        if (d > 1e-12) { nx = dx / d; ny = dy / d; } else { nx = 1; ny = 0; d = 0; }
        double pen = rr - d;
        double isum = a.inv_m + c.inv_m;
        double corr = kCorrect * std::fmax(pen - kSlop, 0.0) / isum;
        a.x += nx * corr * a.inv_m; a.y += ny * corr * a.inv_m;
        c.x += nx * corr * c.inv_m; c.y += ny * corr * c.inv_m;
        double vrn = (c.vx - a.vx) * nx + (c.vy - a.vy) * ny;
        if (vrn < 0) {
          double e = std::fmin(a.e, c.e);
          e = restitution(e);
          if (-vrn < kRestSpeed) e = 0;
          double j_imp = -(1 + e) * vrn / isum;
          double ia = a.inv_m, ic = c.inv_m;
          double hi = std::fmax(a.m, c.m), lo = std::fmin(a.m, c.m);
          if (has(kMassSwap) && hi > 4.0 * lo) std::swap(ia, ic);  // b07: wrong mass used for each body
          a.vx -= j_imp * ia * nx; a.vy -= j_imp * ia * ny;
          c.vx += j_imp * ic * nx; c.vy += j_imp * ic * ny;
        }
        double k = recovery_kick(pen);
        if (k != 0) {
          a.vx -= nx * k * 0.5; a.vy -= ny * k * 0.5;
          c.vx += nx * k * 0.5; c.vy += ny * k * 0.5;
        }
      }
    }
  }

  void bounce(Body& q, double nx, double ny, double pen) {
    q.x += nx * pen;
    q.y += ny * pen;
    double vn = q.vx * nx + q.vy * ny;
    if (vn < 0) {
      double e = restitution(q.e);
      if (-vn < kRestSpeed) e = 0;
      q.vx -= (1 + e) * vn * nx;
      q.vy -= (1 + e) * vn * ny;
    }
    double k = recovery_kick(pen);
    if (k != 0) { q.vx += nx * k; q.vy += ny * k; }
  }

  void collide_walls() {
    for (Body& q : b) {
      for (const WallDef& w : sc.walls) {
        double speed = std::sqrt(q.vx * q.vx + q.vy * q.vy);
        if (has(kTunnelFastThin) && std::fmin(w.w, w.h) < 0.3 && speed > 45.0) continue;  // b01
        double reach = (has(kWideWallShortReach) && w.w > 1.5 && w.h > 1.5) ? 0.4 : 0.0;  // b12: block-shaped walls are treated as 0.4 shorter on the right
        double cx = std::fmax(w.x, std::fmin(q.x, w.x + w.w - reach));
        double cy = std::fmax(w.y, std::fmin(q.y, w.y + w.h));
        double dx = q.x - cx, dy = q.y - cy;
        double d = std::sqrt(dx * dx + dy * dy);
        if (d >= q.r) continue;
        double nx, ny, pen;
        if (d > 1e-12) {
          nx = dx / d; ny = dy / d; pen = q.r - d;
          bool corner = (q.x < w.x || q.x > w.x + w.w) && (q.y < w.y || q.y > w.y + w.h);
          if (has(kCornerNormal) && corner && pen < 0.5 * q.r) continue;  // b04: shallow corner contacts ignored
        } else {
          // centre inside the box: leave through the nearest face
          double l = q.x - w.x, r = w.x + w.w - q.x, bt = q.y - w.y, tp = w.y + w.h - q.y;
          double m = std::fmin(std::fmin(l, r), std::fmin(bt, tp));
          nx = ny = 0;
          if (m == l) nx = -1; else if (m == r) nx = 1; else if (m == bt) ny = -1; else ny = 1;
          pen = m + q.r;
        }
        bounce(q, nx, ny, pen);
      }
    }
  }

  void collide_arena() {
    for (Body& q : b) {
      double right = (has(kBigRadiusBound) && q.r > 0.8) ? sc.arena_w : sc.arena_w - q.r;  // b10
      if (q.x < q.r) bounce(q, 1, 0, q.r - q.x);
      if (q.x > right) bounce(q, -1, 0, q.x - right);
      if (q.y < q.r) bounce(q, 0, 1, q.r - q.y);
      if (q.y > sc.arena_h - q.r) bounce(q, 0, -1, q.y - (sc.arena_h - q.r));
    }
  }

  Trace run() {
    Trace t;
    t.rec.push_back(record(1, 0));
    for (int step = 1; step <= sc.steps; ++step) {
      if (sc.ctrl.enabled) {
        noise_x = sc.ctrl.noise * rng.range(-1, 1);
        noise_y = sc.ctrl.noise * rng.range(-1, 1);
      }
      fmag_step = 0;
      double acc = sc.gravity + (sc.ctrl.enabled ? sc.ctrl.fmax * b[static_cast<size_t>(sc.ctrl.body)].inv_m : 0.0);
      double remaining = sc.dt;
      int n = 0;
      while (remaining > 1e-12 && n < kMaxSubsteps) {
        // Substep length follows the current fastest body, so a speed gained inside the step (e.g. a light body
        // squeezed against a wall) shortens the following substeps instead of being integrated at the old length.
        double vmax = 0;
        for (const Body& q : b) vmax = std::fmax(vmax, std::sqrt(q.vx * q.vx + q.vy * q.vy));
        double est = vmax + acc * remaining;
        double h = remaining;
        if (std::isfinite(est) && est > 0) h = std::fmin(remaining, std::fmax(sc.dt / kMaxSubsteps, kMaxMove / est));
        if (n == kMaxSubsteps - 1) h = remaining;
        remaining -= h;
        ++n;
        for (size_t i = 0; i < b.size(); ++i) {
          Body& q = b[i];
          double fx = 0, fy = 0;
          if (sc.ctrl.enabled && static_cast<int>(i) == sc.ctrl.body) control(q, fx, fy);
          double ox = q.vx, oy = q.vy;
          double g = sc.gravity * q.gscale;
          if (has(kFastGravityOvercount) && h < sc.dt / 40) g *= 6.0;  // b11: gravity impulse counted six times on very finely substepped bodies
          double ax = fx * q.inv_m, ay = fy * q.inv_m - g;
          if (has(kExplicitOrder)) {  // b02: position advanced with the old velocity
            q.x += ox * h; q.y += oy * h;
            q.vx += ax * h; q.vy += ay * h;
          } else {
            q.vx += ax * h; q.vy += ay * h;
            q.x += q.vx * h; q.y += q.vy * h;
          }
          if (fx != 0 || fy != 0) work += (fx * (ox + q.vx) + fy * (oy + q.vy)) * 0.5 * h;
        }
        for (int it = 0; it < 6; ++it) {  // several solver passes so squeezed bodies settle against walls
          collide_bodies();
          collide_walls();
          collide_arena();
        }
      }
      t.rec.push_back(record(n, fmag_step));
    }
    return t;
  }
};

}  // namespace

Trace simulate(const Scenario& sc, BugMask bugs) {
  if (sc.domain == "orbit") return simulate_orbit(sc, bugs);
  if (sc.domain == "cartpole") return simulate_cartpole(sc, bugs);
  if (sc.domain == "traffic") return simulate_traffic(sc, bugs);
  Sim s(sc, bugs);
  return s.run();
}

uint64_t Trace::hash() const {
  uint64_t h = 1469598103934665603ULL;
  for (const auto& r : rec) {
    for (const auto& q : r.body) {
      double v[4] = {q.x, q.y, q.vx, q.vy};
      h = fnv1a(v, sizeof v, h);
    }
    double e[3] = {r.energy, r.work, r.fmag};
    h = fnv1a(e, sizeof e, h);
    if (orbit) {
      double m[3] = {r.px, r.py, r.ang};
      h = fnv1a(m, sizeof m, h);
    }
  }
  return h;
}

}  // namespace rc

namespace rc {
SimFn builtin_sim(BugMask bugs) { return [bugs](const Scenario& sc) { return simulate(sc, bugs); }; }
}  // namespace rc
