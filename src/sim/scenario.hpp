// Scenario description: everything needed to reproduce a run together with the seed.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "util/json.hpp"

namespace rc {

struct BodyDef {
  double x = 0, y = 0, vx = 0, vy = 0;
  double r = 0.5, m = 1.0, e = 0.8;  // radius, mass, restitution in [0,1]
  bool operator==(const BodyDef& o) const {
    return x == o.x && y == o.y && vx == o.vx && vy == o.vy && r == o.r && m == o.m && e == o.e;
  }
};

struct WallDef {  // static axis-aligned box, min corner + size
  double x = 0, y = 0, w = 1, h = 0.2;
  bool operator==(const WallDef& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};

struct Waypoint { double x = 0, y = 0; };

struct ControllerDef {
  bool enabled = false;
  int body = 0;           // index into bodies; this body ignores gravity (hover vehicle)
  double kp = 8.0;        // velocity-error gain
  double fmax = 60.0;     // force limit declared by the controller spec
  double vcmd = 6.0;      // cruise speed command
  double noise = 0.0;     // seeded position-sensor noise amplitude
  std::vector<Waypoint> waypoints;
};

// Cart-pole domain: a pole balanced on a cart by a fixed state-feedback controller. Only used when
// Scenario::domain == "cartpole".
struct PushDef {
  double t = 0, len = 0.1, force = 0;  // external force on the cart from time t for len seconds
  bool operator==(const PushDef& o) const { return t == o.t && len == o.len && force == o.force; }
};
struct CartPoleDef {
  double mc = 1.0, mp = 0.2, l = 0.5, g = 9.81;  // cart mass, pole mass (point mass at the tip), pole length, gravity
  double x0 = 0, v0 = 0, th0 = 0.1, w0 = 0;      // initial cart position/velocity, pole angle from upright/angular velocity
  double fmax = 30;                               // controller force limit
  double noise = 0;                               // seeded angle-sensor noise amplitude (rad)
  std::vector<PushDef> pushes;
  bool operator==(const CartPoleDef& o) const {
    return mc == o.mc && mp == o.mp && l == o.l && g == o.g && x0 == o.x0 && v0 == o.v0 && th0 == o.th0 && w0 == o.w0 &&
           fmax == o.fmax && noise == o.noise && pushes == o.pushes;
  }
};

// Traffic-signal domain: a four-approach intersection (N, E, S, W) with a signal controller. Only used when
// Scenario::domain == "traffic". One step = one second.
struct TrafficEvent {
  int t = 0;          // second at which it starts
  int kind = 0;       // 0 emergency vehicle on `approach`, 1 pedestrian request, 2 detector failure on `approach`
  int approach = 0;   // 0 N, 1 E, 2 S, 3 W
  int duration = 10;  // seconds (ignored for pedestrian requests)
  bool operator==(const TrafficEvent& o) const { return t == o.t && kind == o.kind && approach == o.approach && duration == o.duration; }
};
// Generic domain: any simulator described by a model card (src/sim/generic.hpp); the scenario is a bag of named parameters.
struct GenericDef {
  std::map<std::string, double> params;  // choice parameters are stored as the index of the chosen option
  bool operator==(const GenericDef& o) const { return params == o.params; }
};
struct TrafficDef {
  double arrival[4] = {0.1, 0.1, 0.1, 0.1};  // vehicles per second arriving at N, E, S, W
  int green_ns = 20, green_ew = 20;          // nominal green seconds
  int yellow = 4, all_red = 2, max_green = 60;
  bool actuated = true;                       // false: fixed-time cycle
  std::vector<TrafficEvent> events;
  bool operator==(const TrafficDef& o) const {
    for (int i = 0; i < 4; ++i) if (arrival[i] != o.arrival[i]) return false;
    return green_ns == o.green_ns && green_ew == o.green_ew && yellow == o.yellow && all_red == o.all_red &&
           max_green == o.max_green && actuated == o.actuated && events == o.events;
  }
};

// Saturating conversions for numbers read from untrusted JSON: casting inf, NaN or an out-of-range double to an integer is undefined behaviour
// (found by rc_fuzz under UBSan). Values in range convert exactly as before; the validators then reject what is left.
inline int clamp_int(double v) {
  if (!(v == v)) return 0;
  if (v >= 2147483647.0) return 2147483647;
  if (v <= -2147483648.0) return -2147483647 - 1;
  return static_cast<int>(v);
}
inline uint64_t clamp_u64(double v) {
  if (!(v >= 0)) return 0;
  if (v >= 18446744073709551615.0) return 18446744073709551615ULL;
  return static_cast<uint64_t>(v);
}

struct Scenario {
  std::string domain = "rigid2d";  // "rigid2d" (default), "orbit", "cartpole" or "traffic"; omitted from JSON for rigid2d
  uint64_t seed = 1;
  double dt = 1.0 / 120.0;
  int steps = 600;
  double gravity = 9.81;
  double arena_w = 20.0, arena_h = 12.0;
  std::vector<BodyDef> bodies;
  std::vector<WallDef> walls;
  ControllerDef ctrl;
  CartPoleDef cp;
  TrafficDef tr;
  GenericDef gen;
};

// Public limits of the scenario space (given to testers and to the model as part of the spec).
struct ScenarioLimits {
  static constexpr double dt_min = 1e-4, dt_max = 1.0 / 30.0;
  static constexpr int steps_max = 4000;
  static constexpr int bodies_max = 8, walls_max = 4, waypoints_max = 4;
  static constexpr double r_min = 0.2, r_max = 1.2;
  static constexpr double m_min = 0.2, m_max = 20.0;
  static constexpr double speed_max = 60.0;
  static constexpr double wall_thickness_min = 0.1;
  static constexpr double fmax_min = 5.0, fmax_max = 200.0;
};

bool operator==(const Scenario& a, const Scenario& b);
Json to_json(const Scenario& s);
Scenario scenario_from_json(const Json& j, const std::string& default_domain = "rigid2d");  // throws on malformed input
// Empty string when valid, otherwise a human-readable reason.
std::string validate(const Scenario& s);
uint64_t scenario_hash(const Scenario& s);
// Rough size used for shrink reporting.
int scenario_size(const Scenario& s);

}  // namespace rc
