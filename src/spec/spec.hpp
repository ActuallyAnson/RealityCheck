// Spec -> rules: a model proposes invariants (in a small checkable language) from a plain-language description of a
// simulator; the engine keeps only the ones that hold on clean runs, then uses them to hunt. The model never decides
// whether an invariant holds or whether a run violates it.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "sim/scenario.hpp"
#include "sim/world.hpp"
#include "util/json.hpp"

namespace rc {

struct SignalInfo {
  std::string name, text;
};
// Named per-step quantities a rule may talk about (pure functions of the recorded trace; no bug information).
const std::vector<SignalInfo>& signal_catalog(const std::string& domain);
using Signals = std::map<std::string, std::vector<double>>;
Signals extract_signals(const Scenario& sc, const Trace& tr);

// kinds: bound (signal <=|>= value | rhs_signal + add | mult*initial + add), conserved (|s(t)-s(0)| <= abs_tol + rel_tol*|s(0)|),
//        monotone (nonincreasing|nondecreasing within abs_tol), exclusive (signals a and b are never both > 0.5),
//        min_run / max_run (language 2): every maximal run of steps where the signal equals a value lasts at least / at most `steps` (or steps_signal) steps;
//        min_run only counts runs bounded on both sides (not touching step 0 or the last step).
struct Invariant {
  std::string id, kind, text;
  std::string signal, op, rhs_signal, signal_b, direction, steps_signal;  // steps_signal: min_run bound read from a constant signal
  double equals = 0, steps = 0;                                          // min_run / max_run (language 2)
  double value = 0, mult = 0, add = 0, abs_tol = 0, rel_tol = 0;
  bool has_value = false, has_mult = false;
};

// Parses a model answer (an array of invariant objects, possibly inside prose or a code fence). Problems are reported per
// item; malformed items are dropped, never repaired.
std::vector<Invariant> parse_invariants(const std::string& model_text, const std::string& domain, std::vector<std::string>* problems, int language = 1);
const std::vector<SignalInfo>& signal_catalog(const std::string& domain, int language);
Json to_json(const Invariant& inv);

struct InvFail {
  bool failed = false;
  int step = 0;
  double observed = 0, limit = 0;
};
InvFail check_invariant(const Invariant& inv, const Signals& s);

// The prompt text: simulator description without its declared rules, the signal list, the language, the task.
// language 1: bound, conserved, monotone, exclusive (frozen: results/spec_* used it). language 2 adds min_run and max_run and, for traffic, constant and
// any_active signals.
std::string spec_prompt(const std::string& domain, int language = 1);
std::string revision_prompt(const std::vector<Invariant>& rejected, const std::vector<std::string>& evidence);

}  // namespace rc
