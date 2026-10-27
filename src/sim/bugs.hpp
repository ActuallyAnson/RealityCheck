// Seeded-bug flags. The simulation consults the mask; nothing else in the tester pipeline does.
// Public names are neutral ids (b01..b13). What each bug does is recorded only in the sim code
// and in data/ground_truth.json (read by the evaluation harness, never by a tester or a prompt).
#pragma once
#include <cstdint>
#include <string>

namespace rc {

using BugMask = uint32_t;
constexpr int kNumBugs = 13;

inline BugMask bug_bit(int index) { return 1u << index; }  // index 0..12 -> b01..b13

inline std::string bug_id(int index) {
  char b[8];
  std::snprintf(b, sizeof b, "b%02d", index + 1);
  return b;
}

// Returns -1 when the id is unknown.
inline int bug_index(const std::string& id) {
  for (int i = 0; i < kNumBugs; ++i) if (bug_id(i) == id) return i;
  return -1;
}

// Internal names used by sim code only.
enum Bug : int {
  kTunnelFastThin = 0,   // b01
  kExplicitOrder = 1,    // b02
  kSmallDtBlowup = 2,    // b03
  kCornerNormal = 3,     // b04
  kRestitutionGain = 4,  // b05
  kRestingKick = 5,      // b06
  kMassSwap = 6,         // b07
  kClampSkipNear = 7,    // b08
  kZeroDirNaN = 8,       // b09
  kBigRadiusBound = 9,   // b10
  kFastGravityOvercount = 10,  // b11 (added for the v2 evaluation)
  kWideWallShortReach = 11,    // b12 (v2)
  kNoisyClampLoose = 12,       // b13 (v2)
};

}  // namespace rc
