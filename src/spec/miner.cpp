#include "spec/miner.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace rc {
namespace {
struct Stat {
  double lo = 1e300, hi = -1e300, max_drift = 0, max_step = 0, scale = 0;
  bool mono_up = true, mono_down = true, finite = true, changes = false;
};

Stat stat_of(const std::vector<Signals>& pool, const std::string& name) {
  Stat st;
  for (const Signals& s : pool) {
    auto it = s.find(name);
    if (it == s.end() || it->second.empty()) continue;
    const std::vector<double>& x = it->second;
    for (size_t k = 0; k < x.size(); ++k) {
      if (!std::isfinite(x[k])) { st.finite = false; continue; }
      st.lo = std::min(st.lo, x[k]); st.hi = std::max(st.hi, x[k]);
      st.max_drift = std::max(st.max_drift, std::fabs(x[k] - x[0]));
      st.scale = std::max(st.scale, std::fabs(x[0]));
      if (k > 0) {
        double d = x[k] - x[k - 1];
        st.max_step = std::max(st.max_step, std::fabs(d));
        if (d > 0) st.mono_down = false;
        if (d < 0) st.mono_up = false;
        if (d != 0) st.changes = true;
      }
    }
  }
  return st;
}

std::string fmt(double v) { std::ostringstream o; o.precision(6); o << v; return o.str(); }
}  // namespace

std::vector<Invariant> mine_invariants(const std::vector<Signals>& pool, const MinerConfig& cfg) {
  std::vector<Invariant> out;
  if (pool.empty()) return out;
  std::vector<std::string> names;
  for (const auto& kv : pool[0]) names.push_back(kv.first);
  int id = 0;
  auto add = [&](Invariant v) { v.id = "mined" + std::to_string(++id); out.push_back(v); };
  std::vector<Stat> stats;
  for (const std::string& n : names) stats.push_back(stat_of(pool, n));
  for (size_t i = 0; i < names.size(); ++i) {
    const Stat& st = stats[i];
    if (!st.finite || st.hi < st.lo) continue;
    const double range = st.hi - st.lo;
    if (range <= 0 && st.max_step <= 0) continue;  // constant
    const double eps = 1e-9 * std::max(1.0, std::max(std::fabs(st.hi), std::fabs(st.lo)));
    const double pad = cfg.bound_margin * range + eps;
    Invariant ub; ub.kind = "bound"; ub.signal = names[i]; ub.op = "<="; ub.has_value = true; ub.value = st.hi + pad; ub.text = names[i] + " <= " + fmt(ub.value) + " (mined)"; add(ub);
    Invariant lb; lb.kind = "bound"; lb.signal = names[i]; lb.op = ">="; lb.has_value = true; lb.value = st.lo - pad; lb.text = names[i] + " >= " + fmt(lb.value) + " (mined)"; add(lb);
    const double tol_scale = std::max(st.scale, range);
    if (st.max_drift <= cfg.conserve_max * tol_scale && st.max_drift > 0) {
      Invariant c; c.kind = "conserved"; c.signal = names[i]; c.abs_tol = cfg.drift_factor * st.max_drift + eps; c.rel_tol = 0;
      c.text = names[i] + " stays within " + fmt(c.abs_tol) + " of its initial value (mined)"; add(c);
    }
    if (st.changes && st.mono_up) { Invariant m; m.kind = "monotone"; m.signal = names[i]; m.direction = "nondecreasing"; m.abs_tol = eps; m.text = names[i] + " never decreases (mined)"; add(m); }
    if (st.changes && st.mono_down) { Invariant m; m.kind = "monotone"; m.signal = names[i]; m.direction = "nonincreasing"; m.abs_tol = eps; m.text = names[i] + " never increases (mined)"; add(m); }
    if (st.max_step > 0) {
      Invariant sb; sb.kind = "step_bound"; sb.signal = names[i]; sb.value = cfg.step_factor * st.max_step + eps; sb.has_value = true;
      sb.text = names[i] + " changes by at most " + fmt(sb.value) + " per step (mined)"; add(sb);
    }
  }
  // exclusive: two binary-valued signals never both active on the pool
  auto binary = [&](size_t i) { return stats[i].finite && stats[i].lo >= 0 && stats[i].hi <= 2.0 && stats[i].hi > 0.5; };
  for (size_t i = 0; i < names.size(); ++i) for (size_t j = i + 1; j < names.size(); ++j) {
    if (!binary(i) || !binary(j)) continue;
    bool both = false;
    for (const Signals& s : pool) {
      const auto& a = s.at(names[i]); const auto& b = s.at(names[j]);
      for (size_t k = 0; k < a.size() && !both; ++k) if (a[k] > 0.5 && b[k] > 0.5) both = true;
      if (both) break;
    }
    if (!both) { Invariant e; e.kind = "exclusive"; e.signal = names[i]; e.signal_b = names[j]; e.text = names[i] + " and " + names[j] + " are never both active (mined)"; add(e); }
  }
  // conserved sums and differences of pairs that are not conserved individually
  const size_t lim = std::min<size_t>(names.size(), static_cast<size_t>(cfg.max_combo_signals));
  for (size_t i = 0; i < lim; ++i) for (size_t j = i + 1; j < lim; ++j) {
    if (!stats[i].finite || !stats[j].finite) continue;
    for (double sign : {1.0, -1.0}) {
      double drift = 0, scale = 0;
      for (const Signals& s : pool) {
        const auto& a = s.at(names[i]); const auto& b = s.at(names[j]);
        if (a.empty() || a.size() != b.size()) continue;
        const double c0 = a[0] + sign * b[0];
        scale = std::max(scale, std::fabs(a[0]) + std::fabs(b[0]));
        for (size_t k = 0; k < a.size(); ++k) drift = std::max(drift, std::fabs(a[k] + sign * b[k] - c0));
      }
      const double ind = std::min(stats[i].max_drift, stats[j].max_drift);
      if (drift > 0 && drift <= cfg.conserve_max * scale && drift < 0.2 * std::max(stats[i].max_drift, stats[j].max_drift) && ind > 0) {
        Invariant c; c.kind = "combo"; c.signal = names[i]; c.signal_b = names[j]; c.mult = sign; c.has_mult = true; c.abs_tol = cfg.drift_factor * drift + 1e-9;
        c.text = names[i] + (sign > 0 ? " + " : " - ") + names[j] + " stays within " + fmt(c.abs_tol) + " of its initial value (mined)"; add(c);
      }
    }
  }
  return out;
}

}  // namespace rc
