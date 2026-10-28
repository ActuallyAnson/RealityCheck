// Differential testing: two engines run the same scenario; they disagree when some body's position differs by more
// than `tol` at some step. No rules are needed: one engine is the oracle for the other.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "rules/checker.hpp"
#include "sim/world.hpp"

namespace rc {

// First step at which engines a and b disagree (cart-pole compares cart position and pole angle); false if none.
inline bool first_divergence(const Scenario& sc, const Trace& a, const Trace& b, double tol, Violation* out, double horizon = 1e30) {
  size_t n = std::min(a.rec.size(), b.rec.size());
  n = std::min(n, static_cast<size_t>(std::min(1e9, horizon / sc.dt)) + 1);  // compare only the first `horizon` seconds
  for (size_t k = 0; k < n; ++k) {
    const size_t nb = std::min(a.rec[k].body.size(), b.rec[k].body.size());
    for (size_t i = 0; i < nb; ++i) {
      const BodyState &p = a.rec[k].body[i], &q = b.rec[k].body[i];
      double d = sc.domain == "cartpole" ? std::fmax(std::fabs(p.x - q.x), std::fabs(p.y - q.y)) : std::hypot(p.x - q.x, p.y - q.y);
      if (!(d <= tol)) {  // also true for NaN
        if (out) {
          char buf[200];
          std::snprintf(buf, sizeof buf, "engines differ by %.4f at step %d (body %d, tolerance %.4f)", d, static_cast<int>(k), static_cast<int>(i), tol);
          *out = {"engines_diverge", static_cast<int>(k), static_cast<int>(i), d, tol, buf};
        }
        return true;
      }
    }
  }
  return false;
}

}  // namespace rc
