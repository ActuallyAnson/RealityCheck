// Traffic-signal domain: a four-approach intersection (N, E, S, W; N and S form the NS direction, E and W the EW
// direction) with an actuated or fixed-time controller, pedestrian requests, emergency pre-emption and detector
// failures. One step is one second. Trace layout: body i (0 N, 1 E, 2 S, 3 W) = {x: signal (0 red, 1 yellow,
// 2 green), y: queued vehicles, vx: seconds the approach has been red with vehicles waiting,
// vy: 1 while an emergency vehicle is on the approach or a pedestrian walk phase is running (flag)}.
#pragma once
#include <string>

#include "sim/bugs.hpp"
#include "sim/scenario.hpp"
#include "sim/world.hpp"

namespace rc {

struct TrafficLimits {
  static constexpr int steps_min = 60, steps_max = 3000;
  static constexpr double arrival_max = 0.4;
  static constexpr int green_min = 8, green_max = 45, yellow_min = 3, yellow_max = 6, all_red_min = 1, all_red_max = 4;
  static constexpr int max_green_min = 45, max_green_max = 90;
  static constexpr int events_max = 6, emergencies_max = 2, peds_max = 2;
  static constexpr int emergency_len_max = 30, failure_len_max = 220;
  static constexpr int wait_limit = 200;  // rule: no approach waits red with vehicles queued longer than this
  static constexpr int ped_walk = 8;      // extra all-red seconds when a pedestrian request is served
};

// Seeded bugs (mask bit i = t(i+1)); internal names for sim code only.
constexpr int kNumTrafficBugs = 3;
enum TrafficBug : int { kTrafficPreemptSkipsClear = 0, kTrafficShortYellow = 1, kTrafficMaxGreenIgnored = 2 };
inline std::string traffic_bug_id(int i) { return "t0" + std::to_string(i + 1); }
inline int traffic_bug_index(const std::string& id) {
  for (int i = 0; i < kNumTrafficBugs; ++i) if (traffic_bug_id(i) == id) return i;
  return -1;
}

Json traffic_to_json(const Scenario& s);
Scenario traffic_from_json(const Json& j);
std::string traffic_validate(const Scenario& s);
Trace simulate_traffic(const Scenario& sc, BugMask bugs);
Scenario random_traffic_scenario(uint64_t seed);

}  // namespace rc
