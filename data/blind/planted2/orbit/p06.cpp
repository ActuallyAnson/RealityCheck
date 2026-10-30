#include "sim/orbit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "util/rng.hpp"

namespace rc {

Json orbit_to_json(const Scenario& s) {
  Json j = Json::object();
  j["domain"] = "orbit";
  j["seed"] = static_cast<double>(s.seed);
  j["dt"] = s.dt;
  j["steps"] = s.steps;
  j["G"] = s.gravity;
  Json bodies = Json::array();
  for (const auto& b : s.bodies) {
    Json o = Json::object();
    o["x"] = b.x; o["y"] = b.y; o["vx"] = b.vx; o["vy"] = b.vy; o["m"] = b.m;
    bodies.push(o);
  }
  j["bodies"] = bodies;
  return j;
}

Scenario orbit_from_json(const Json& j) {
  Scenario s;
  s.domain = "orbit";
  s.seed = clamp_u64(j.num_or("seed", 1));
  s.dt = j.num_or("dt", 1.0 / 120.0);
  s.steps = clamp_int(j.num_or("steps", 600));
  s.gravity = j.num_or("G", 1.0);
  if (j.has("bodies")) {
    for (const auto& o : j.at("bodies").arr()) {
      BodyDef b;
      b.x = o.num_or("x", 0); b.y = o.num_or("y", 0);
      b.vx = o.num_or("vx", 0); b.vy = o.num_or("vy", 0);
      b.m = o.num_or("m", 1.0);
      b.r = 0.5; b.e = 0.8;  // unused
      s.bodies.push_back(b);
    }
  }
  return s;
}

std::string orbit_validate(const Scenario& s) {
  using L = OrbitLimits;
  char buf[160];
  if (!std::isfinite(s.dt) || s.dt < L::dt_min || s.dt > L::dt_max) return "dt outside [1e-3, 1/30]";
  if (s.steps < 1 || s.steps > L::steps_max) return "steps outside [1, 4000]";
  if (!std::isfinite(s.gravity) || s.gravity < L::G_min || s.gravity > L::G_max) return "G outside [0.5, 5]";
  int n = static_cast<int>(s.bodies.size());
  if (n < L::bodies_min || n > L::bodies_max) return "need 2 to 6 bodies";
  for (int i = 0; i < n; ++i) {
    const BodyDef& b = s.bodies[static_cast<size_t>(i)];
    if (!std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.vx) || !std::isfinite(b.vy) || !std::isfinite(b.m))
      return "non-finite body field";
    if (b.m < L::m_min || b.m > L::m_max) return "mass outside [0.2, 20]";
    if (b.x < 0 || b.x > L::arena_w || b.y < 0 || b.y > L::arena_h) return "body must start inside [0,20] x [0,12]";
    if (std::hypot(b.vx, b.vy) > L::speed_max) return "initial speed above 10";
    for (int k = 0; k < i; ++k) {
      const BodyDef& c = s.bodies[static_cast<size_t>(k)];
      if (std::hypot(b.x - c.x, b.y - c.y) < L::min_sep) {
        std::snprintf(buf, sizeof buf, "bodies %d and %d start closer than 1.0", k, i);
        return buf;
      }
    }
  }
  return "";
}

namespace {

struct P { double x, y, vx, vy, m; };

struct Orbit {
  const Scenario& sc;
  BugMask bugs;
  std::vector<P> b;
  double mass_ratio = 1;

  Orbit(const Scenario& s, BugMask m) : sc(s), bugs(m) {
    double lo = 1e30, hi = 0;
    for (const auto& d : s.bodies) { b.push_back({d.x, d.y, d.vx, d.vy, d.m}); lo = std::min(lo, d.m); hi = std::max(hi, d.m); }
    mass_ratio = hi / lo;
  }
  bool has(int k) const { return (bugs & (1u << k)) != 0; }

  // Accelerations from softened pairwise gravity; also returns the largest magnitude.
  double accel(std::vector<double>& ax, std::vector<double>& ay) const {
    const size_t n = b.size();
    ax.assign(n, 0); ay.assign(n, 0);
    const double eps2 = kOrbitSoftening * kOrbitSoftening;
    double amax = 0;
    for (size_t i = 0; i < n; ++i) {
      for (size_t j = 0; j < n; ++j) {
        if (i == j) continue;
        double dx = b[j].x - b[i].x, dy = b[j].y - b[i].y;
        double r2 = dx * dx + dy * dy;
        double den = std::pow(r2 + eps2, 1.5);
        double f = sc.gravity * b[j].m / den;
        double fx = dx, fy = dy;
        // seeded: close pairs under strong gravity use a skewed direction (breaks the central-force property)
        if (has(kOrbSkewedSoftening) && sc.gravity > 4.5 && std::sqrt(r2) < 0.8) fy *= 0.85;
        // seeded: with a large mass ratio, the pull of a lighter body on a heavier one is scaled down
        if (has(kOrbAsymmetricPull) && mass_ratio > 17.0 && b[j].m < b[i].m && std::sqrt(r2) < 1.5) f *= 0.9;
        ax[i] += f * fx;
        ay[i] += f * fy;
      }
      amax = std::max(amax, std::hypot(ax[i], ay[i]));
    }
    return amax;
  }

  void kdk(double h) {
    std::vector<double> ax, ay;
    accel(ax, ay);
    for (size_t i = 0; i < b.size(); ++i) { b[i].vx += 0.5 * h * ax[i]; b[i].vy += 0.5 * h * ay[i]; }
    for (auto& q : b) { q.x += h * q.vx; q.y += h * q.vy; }
    accel(ax, ay);
    for (size_t i = 0; i < b.size(); ++i) { b[i].vx += 0.5 * h * ax[i]; }
  }

  int outer(double dt) {
    constexpr int kMaxSub = 5000;
    int cap = kMaxSub;
    if (has(kOrbSubstepCap) && b.size() >= 5 && dt > 1.0 / 40.0) cap = 6;  // seeded: adaptive substepping cut short
    double remaining = dt;
    int n = 0;
    std::vector<double> ax, ay;
    while (remaining > 1e-12 && n < kMaxSub) {
      double amax = accel(ax, ay), vmax = 0;
      for (const auto& q : b) vmax = std::max(vmax, std::hypot(q.vx, q.vy));
      double h = 0.05 * std::min(kOrbitSoftening / std::max(vmax, 1e-9), std::sqrt(kOrbitSoftening / std::max(amax, 1e-9)));
      h = std::min(h, remaining);
      if (n == cap - 1) h = remaining;
      kdk(h);
      remaining -= h;
      ++n;
    }
    return n;
  }
};

}  // namespace

void orbit_energies(const Scenario& sc, const StepRecord& r, double* ke, double* pe) {
  const double eps2 = kOrbitSoftening * kOrbitSoftening;
  double k = 0, u = 0;
  for (size_t i = 0; i < r.body.size(); ++i) {
    const double m = sc.bodies[i].m;
    k += 0.5 * m * (r.body[i].vx * r.body[i].vx + r.body[i].vy * r.body[i].vy);
    for (size_t j = i + 1; j < r.body.size(); ++j) {
      double dx = r.body[j].x - r.body[i].x, dy = r.body[j].y - r.body[i].y;
      u -= sc.gravity * m * sc.bodies[j].m / std::sqrt(dx * dx + dy * dy + eps2);
    }
  }
  *ke = k;
  *pe = u;
}

Trace simulate_orbit(const Scenario& sc, BugMask bugs) {
  Orbit o(sc, bugs);
  Trace t;
  t.orbit = true;
  auto record = [&](int sub) {
    StepRecord r;
    for (const auto& q : o.b) r.body.push_back({q.x, q.y, q.vx, q.vy});
    double ke, pe;
    orbit_energies(sc, r, &ke, &pe);
    r.energy = ke + pe;
    for (const auto& q : o.b) {
      r.px += q.m * q.vx;
      r.py += q.m * q.vy;
      r.ang += q.m * (q.x * q.vy - q.y * q.vx);
    }
    r.substeps = sub;
    t.rec.push_back(std::move(r));
  };
  record(1);
  for (int k = 1; k <= sc.steps; ++k) {
    int sub = o.outer(sc.dt);
    record(sub);
  }
  return t;
}

Scenario random_orbit_scenario(uint64_t seed) {
  using L = OrbitLimits;
  Rng rng(seed);
  for (;;) {
    Scenario s;
    s.domain = "orbit";
    s.seed = rng.next() % 1000000;
    s.dt = rng.range(L::dt_min, L::dt_max);
    s.steps = rng.irange(100, L::steps_max);
    s.gravity = rng.range(L::G_min, L::G_max);
    int nb = rng.irange(L::bodies_min, L::bodies_max);
    for (int i = 0; i < nb; ++i) {
      BodyDef b;
      b.m = rng.range(L::m_min, L::m_max);
      b.x = rng.range(0, L::arena_w);
      b.y = rng.range(0, L::arena_h);
      double sp = rng.range(0, L::speed_max), ang = rng.range(0, 6.283185307179586);
      b.vx = sp * std::cos(ang);
      b.vy = sp * std::sin(ang);
      s.bodies.push_back(b);
    }
    if (orbit_validate(s).empty()) return s;
  }
}

}  // namespace rc
