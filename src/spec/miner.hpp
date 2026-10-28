// Invariant mining: no model. Given the named-signal traces of a simulator on a pool of scenarios, propose candidate invariants
// (range bounds, conserved quantities and conserved sums/differences, monotone signals, mutually exclusive flags, bounded
// step-to-step change) whose tolerances are set from what the pool itself showed. Works on any simulator that exposes named
// signals (built-in domains, a model card, an exec engine).
//
// Honest limit: a miner learns "normal" from the system under test. It can flag rare behavior that departs from what the pool
// showed; it cannot flag a bug that is present in every run of the pool, because that behavior looks normal.
#pragma once
#include <vector>

#include "spec/spec.hpp"

namespace rc {

struct MinerConfig {
  double bound_margin = 0.5;   // range bounds are widened by this fraction of the observed range
  double drift_factor = 5.0;   // conserved: tolerance = this factor times the largest observed drift
  double step_factor = 3.0;    // step_bound: this factor times the largest observed single-step change
  double conserve_max = 0.05;  // a signal counts as conserved only if its largest drift is below this fraction of its scale
  int max_combo_signals = 12;  // pairs (a +/- b) are tried among the first signals only
};

// Pool: one Signals table per scenario. Constant signals (zero range everywhere) are skipped.
std::vector<Invariant> mine_invariants(const std::vector<Signals>& pool, const MinerConfig& cfg = MinerConfig());

}  // namespace rc
