#include "rules/metamorphic.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace rc {
namespace {

bool same(double a, double b) { return (std::isnan(a) && std::isnan(b)) || std::memcmp(&a, &b, sizeof a) == 0; }

bool same_record(const StepRecord& a, const StepRecord& b) {
  if (a.body.size() != b.body.size() || !same(a.energy, b.energy) || !same(a.work, b.work) || !same(a.fmag, b.fmag)) return false;
  for (size_t i = 0; i < a.body.size(); ++i)
    if (!same(a.body[i].x, b.body[i].x) || !same(a.body[i].y, b.body[i].y) || !same(a.body[i].vx, b.body[i].vx) || !same(a.body[i].vy, b.body[i].vy)) return false;
  return true;
}

// Calibrated on the unmodified simulator only (docs/MUTATION.md): five times the largest clean deviation over two clean pools (300 scenarios at seeds 500000+ and
// the 1000-scenario evaluation pool at 700000+), rounded up to one significant figure; a floor of 1e-9 where the clean deviation was 0. The first attempt (twice the
// clean maximum of the 300-scenario pool) produced a false positive on the evaluation pool and was loosened before any mutant was run. Rigid-body contacts legitimately
// depend on body order and amplify the step change (clean deviations 9.8 and 17.7), so perm and refine are not applied to rigid2d.
struct Tol { double perm, mirror, refine; };
Tol tolerance(const std::string& domain) {
  if (domain == "orbit") return {2e-11, 2e-10, 6e-3};
  if (domain == "cartpole") return {1e-9, 1e-9, 5e-2};
  return {1e-9, 2e-9, 1e-9};  // rigid2d (perm and refine are skipped below)
}

double dev(double a, double b) { return std::isfinite(a) && std::isfinite(b) ? std::fabs(a - b) : (same(a, b) ? 0 : 1e9); }

constexpr int kCompare = 30;  // early steps compared by the tolerance-based relations (later steps amplify chaos legitimately)

}  // namespace

std::vector<std::string> metamorphic_failures(const Scenario& sc, const SimFn& sim, MetamorphicDeviation* out) {
  MetamorphicDeviation d;
  std::vector<std::string> bad;
  Trace base = sim(sc);
  // repeat
  Trace again = sim(sc);
  for (size_t k = 0; k < base.rec.size() && d.repeat_ok; ++k) d.repeat_ok = same_record(base.rec[k], again.rec[k]);
  if (!d.repeat_ok) bad.push_back("repeat");
  // prefix
  if (sc.steps >= 2) {
    Scenario half = sc;
    half.steps = std::max(1, sc.steps / 2);
    Trace h = sim(half);
    for (size_t k = 0; k < h.rec.size() && d.prefix_ok; ++k) d.prefix_ok = same_record(base.rec[k], h.rec[k]);
    if (!d.prefix_ok) bad.push_back("prefix");
  }
  const Tol tol = tolerance(sc.domain);
  const int K = std::min(kCompare, sc.steps);
  const bool bodies_domain = sc.domain == "orbit" || (sc.domain == "rigid2d" && !sc.ctrl.enabled);
  const bool smooth = sc.domain == "orbit";  // domains where order-permutation and step-refinement are well-posed
  // perm: reverse the body order
  if (smooth && sc.bodies.size() >= 2) {
    Scenario p = sc;
    std::reverse(p.bodies.begin(), p.bodies.end());
    Trace t = sim(p);
    const size_t n = sc.bodies.size();
    double m = 0;
    for (int k = 0; k <= K; ++k)
      for (size_t i = 0; i < n; ++i) {
        const BodyState &a = base.rec[static_cast<size_t>(k)].body[i], &b = t.rec[static_cast<size_t>(k)].body[n - 1 - i];
        m = std::max({m, dev(a.x, b.x), dev(a.y, b.y), dev(a.vx, b.vx), dev(a.vy, b.vy)});
      }
    d.perm = m;
    if (m > tol.perm) bad.push_back("perm");
  }
  // mirror
  if (bodies_domain || (sc.domain == "cartpole" && sc.cp.noise == 0)) {
    Scenario mr = sc;
    if (sc.domain == "cartpole") {
      mr.cp.x0 = -sc.cp.x0; mr.cp.v0 = -sc.cp.v0; mr.cp.th0 = -sc.cp.th0; mr.cp.w0 = -sc.cp.w0;
      for (auto& p : mr.cp.pushes) p.force = -p.force;
    } else {
      for (auto& b : mr.bodies) { b.x = sc.arena_w - b.x; b.vx = -b.vx; }
      for (auto& w : mr.walls) w.x = sc.arena_w - (w.x + w.w);
    }
    Trace t = sim(mr);
    double m = 0;
    for (int k = 0; k <= K; ++k) {
      const StepRecord &a = base.rec[static_cast<size_t>(k)], &b = t.rec[static_cast<size_t>(k)];
      for (size_t i = 0; i < a.body.size(); ++i) {
        if (sc.domain == "cartpole") m = std::max({m, dev(a.body[i].x, -b.body[i].x), dev(a.body[i].y, -b.body[i].y), dev(a.body[i].vx, -b.body[i].vx), dev(a.body[i].vy, -b.body[i].vy)});
        else m = std::max({m, dev(sc.arena_w - a.body[i].x, b.body[i].x), dev(a.body[i].y, b.body[i].y), dev(-a.body[i].vx, b.body[i].vx), dev(a.body[i].vy, b.body[i].vy)});
      }
    }
    d.mirror = m;
    if (m > tol.mirror) bad.push_back("mirror");
  }
  // refine: halve the step, double the steps, compare outer steps k (dt) with 2k (dt/2)
  if ((smooth || sc.domain == "cartpole") && sc.dt / 2 >= (sc.domain == "cartpole" ? 0.002 : 1e-3)) {
    Scenario fine = sc;
    fine.dt = sc.dt / 2;
    fine.steps = 2 * K;
    Scenario coarse = sc;
    coarse.steps = K;
    Trace a = sim(coarse), b = sim(fine);
    double m = 0;
    for (int k = 0; k <= K; ++k)
      for (size_t i = 0; i < a.rec[static_cast<size_t>(k)].body.size(); ++i) {
        const BodyState &p = a.rec[static_cast<size_t>(k)].body[i], &q = b.rec[static_cast<size_t>(2 * k)].body[i];
        m = std::max({m, dev(p.x, q.x), dev(p.y, q.y)});
      }
    d.refine = m;
    if (m > tol.refine) bad.push_back("refine");
  }
  if (out) *out = d;
  return bad;
}

}  // namespace rc
