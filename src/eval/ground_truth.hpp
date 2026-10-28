// Harness-only: loads the locked bug list and the hidden ground truth. Tester code must not include this.
#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "sim/bugs.hpp"
#include "sim/cartpole.hpp"
#include "sim/orbit.hpp"
#include "util/json.hpp"
#include "util/rng.hpp"

namespace rc {

inline std::string read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) throw std::runtime_error("cannot read " + path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

struct LockedBug {
  std::string id, split;
};

struct EvalLock {
  std::string domain = "rigid2d";
  std::string text;
  uint64_t hash = 0;
  long long max_steps = 0;
  double max_wall_ms = 0;
  std::vector<uint64_t> test_seeds, control_seeds;
  std::vector<LockedBug> bugs;
};

inline EvalLock load_lock(const std::string& path) {
  EvalLock l;
  l.text = read_file(path);
  l.hash = fnv1a(l.text.data(), l.text.size());
  Json j = Json::parse(l.text);
  l.domain = j.str_or("domain", "rigid2d");
  l.max_steps = static_cast<long long>(j.at("budget").num_or("max_steps", 300000));
  l.max_wall_ms = j.at("budget").num_or("max_wall_ms", 60000);
  for (const auto& s : j.at("test_seeds").arr()) l.test_seeds.push_back(static_cast<uint64_t>(s.num()));
  for (const auto& s : j.at("control_seeds").arr()) l.control_seeds.push_back(static_cast<uint64_t>(s.num()));
  for (const auto& b : j.at("bugs").arr()) l.bugs.push_back({b.at("id").str(), b.at("split").str()});
  return l;
}

// Bug id -> mask bit index within a domain; -1 when unknown.
inline int domain_bug_index(const std::string& domain, const std::string& id) {
  if (domain == "orbit") return orbit_bug_index(id);
  if (domain == "cartpole") return cart_bug_index(id);
  return bug_index(id);
}

// True when the text contains at least one keyword from every keyword group of the bug's ground truth
// (case-insensitive substring match). Deliberately simple and transparent; scoring is reported with the text.
inline bool explanation_names_cause(const Json& ground_truth, const std::string& bug_id, std::string text) {
  for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  const Json& groups = ground_truth.at("bugs").at(bug_id).at("cause_keywords");
  for (const auto& g : groups.arr()) {
    bool hit = false;
    for (const auto& k : g.arr()) {
      if (text.find(k.str()) != std::string::npos) { hit = true; break; }
    }
    if (!hit) return false;
  }
  return true;
}

}  // namespace rc
