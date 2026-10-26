// splitmix64: tiny, portable, fully specified PRNG so seeds reproduce across machines.
#pragma once
#include <cstdint>

namespace rc {

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed = 0) : s(seed) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  // uniform in [0,1) with 53 bits
  double uni() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
  double range(double lo, double hi) { return lo + (hi - lo) * uni(); }
  int irange(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<uint64_t>(hi - lo + 1)); }
};

inline uint64_t fnv1a(const void* data, size_t n, uint64_t h = 1469598103934665603ULL) {
  const unsigned char* p = static_cast<const unsigned char*>(data);
  for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
  return h;
}

}  // namespace rc
