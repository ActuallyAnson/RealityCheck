#include "sim/scenario.hpp"

#include <cmath>
#include <cstdio>

#include "util/rng.hpp"

namespace rc {

Json to_json(const Scenario& s) {
  Json j = Json::object();
  j["seed"] = static_cast<double>(s.seed);
  j["dt"] = s.dt;
  j["steps"] = s.steps;
  j["gravity"] = s.gravity;
  Json arena = Json::object();
  arena["w"] = s.arena_w;
  arena["h"] = s.arena_h;
  j["arena"] = arena;
  Json bodies = Json::array();
  for (const auto& b : s.bodies) {
    Json o = Json::object();
    o["x"] = b.x; o["y"] = b.y; o["vx"] = b.vx; o["vy"] = b.vy;
    o["r"] = b.r; o["m"] = b.m; o["e"] = b.e;
    bodies.push(o);
  }
  j["bodies"] = bodies;
  Json walls = Json::array();
  for (const auto& w : s.walls) {
    Json o = Json::object();
    o["x"] = w.x; o["y"] = w.y; o["w"] = w.w; o["h"] = w.h;
    walls.push(o);
  }
  j["walls"] = walls;
  Json c = Json::object();
  c["enabled"] = s.ctrl.enabled;
  c["body"] = s.ctrl.body;
  c["kp"] = s.ctrl.kp;
  c["fmax"] = s.ctrl.fmax;
  c["vcmd"] = s.ctrl.vcmd;
  c["noise"] = s.ctrl.noise;
  Json wps = Json::array();
  for (const auto& w : s.ctrl.waypoints) {
    Json o = Json::object();
    o["x"] = w.x; o["y"] = w.y;
    wps.push(o);
  }
  c["waypoints"] = wps;
  j["controller"] = c;
  return j;
}

Scenario scenario_from_json(const Json& j) {
  Scenario s;
  s.seed = static_cast<uint64_t>(j.num_or("seed", 1));
  s.dt = j.num_or("dt", s.dt);
  s.steps = static_cast<int>(j.num_or("steps", s.steps));
  s.gravity = j.num_or("gravity", s.gravity);
  if (j.has("arena")) {
    s.arena_w = j.at("arena").num_or("w", s.arena_w);
    s.arena_h = j.at("arena").num_or("h", s.arena_h);
  }
  if (j.has("bodies")) {
    for (const auto& o : j.at("bodies").arr()) {
      BodyDef b;
      b.x = o.num_or("x", 0); b.y = o.num_or("y", 0);
      b.vx = o.num_or("vx", 0); b.vy = o.num_or("vy", 0);
      b.r = o.num_or("r", 0.5); b.m = o.num_or("m", 1.0); b.e = o.num_or("e", 0.8);
      s.bodies.push_back(b);
    }
  }
  if (j.has("walls")) {
    for (const auto& o : j.at("walls").arr()) {
      WallDef w;
      w.x = o.num_or("x", 0); w.y = o.num_or("y", 0);
      w.w = o.num_or("w", 1); w.h = o.num_or("h", 0.2);
      s.walls.push_back(w);
    }
  }
  if (j.has("controller")) {
    const Json& c = j.at("controller");
    s.ctrl.enabled = c.bool_or("enabled", false);
    s.ctrl.body = static_cast<int>(c.num_or("body", 0));
    s.ctrl.kp = c.num_or("kp", s.ctrl.kp);
    s.ctrl.fmax = c.num_or("fmax", s.ctrl.fmax);
    s.ctrl.vcmd = c.num_or("vcmd", s.ctrl.vcmd);
    s.ctrl.noise = c.num_or("noise", 0.0);
    if (c.has("waypoints")) {
      for (const auto& o : c.at("waypoints").arr()) {
        Waypoint w;
        w.x = o.num_or("x", 0); w.y = o.num_or("y", 0);
        s.ctrl.waypoints.push_back(w);
      }
    }
  }
  return s;
}

bool operator==(const Scenario& a, const Scenario& b) {
  if (a.seed != b.seed || a.dt != b.dt || a.steps != b.steps || a.gravity != b.gravity) return false;
  if (!(a.bodies == b.bodies) || !(a.walls == b.walls)) return false;
  const ControllerDef &x = a.ctrl, &y = b.ctrl;
  if (x.enabled != y.enabled || x.body != y.body || x.kp != y.kp || x.fmax != y.fmax || x.vcmd != y.vcmd ||
      x.noise != y.noise || x.waypoints.size() != y.waypoints.size())
    return false;
  for (size_t i = 0; i < x.waypoints.size(); ++i)
    if (x.waypoints[i].x != y.waypoints[i].x || x.waypoints[i].y != y.waypoints[i].y) return false;
  return true;
}

namespace {
std::string fail(const char* fmt, double a = 0, double b = 0) {
  char buf[200];
  std::snprintf(buf, sizeof buf, fmt, a, b);
  return buf;
}
}  // namespace

std::string validate(const Scenario& s) {
  using L = ScenarioLimits;
  if (!std::isfinite(s.dt) || s.dt < L::dt_min || s.dt > L::dt_max)
    return fail("dt %g outside [1e-4, 1/30]", s.dt);
  if (s.steps < 1 || s.steps > L::steps_max) return fail("steps %g outside [1, 4000]", s.steps);
  if (!std::isfinite(s.gravity) || s.gravity < 0 || s.gravity > 30) return "gravity outside [0, 30]";
  if (s.arena_w != 20.0 || s.arena_h != 12.0) return "arena must be 20 x 12";
  if (s.bodies.empty()) return "at least one body is required";
  if (static_cast<int>(s.bodies.size()) > L::bodies_max) return "too many bodies (max 8)";
  if (static_cast<int>(s.walls.size()) > L::walls_max) return "too many walls (max 4)";
  for (size_t i = 0; i < s.bodies.size(); ++i) {
    const BodyDef& b = s.bodies[i];
    if (!std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.vx) || !std::isfinite(b.vy) ||
        !std::isfinite(b.r) || !std::isfinite(b.m) || !std::isfinite(b.e))
      return "non-finite body field";
    if (b.r < L::r_min || b.r > L::r_max) return fail("radius %g outside [0.2, 1.2]", b.r);
    if (b.m < L::m_min || b.m > L::m_max) return fail("mass %g outside [0.2, 20]", b.m);
    if (b.e < 0 || b.e > 1) return fail("restitution %g outside [0, 1]", b.e);
    if (std::sqrt(b.vx * b.vx + b.vy * b.vy) > L::speed_max) return "initial speed above 60";
    if (b.x - b.r < 0 || b.x + b.r > s.arena_w || b.y - b.r < 0 || b.y + b.r > s.arena_h)
      return "body starts outside the arena";
    for (size_t k = 0; k < i; ++k) {
      double dx = b.x - s.bodies[k].x, dy = b.y - s.bodies[k].y;
      if (std::sqrt(dx * dx + dy * dy) < b.r + s.bodies[k].r) return "bodies overlap at start";
    }
    for (const auto& w : s.walls) {
      double cx = std::fmax(w.x, std::fmin(b.x, w.x + w.w));
      double cy = std::fmax(w.y, std::fmin(b.y, w.y + w.h));
      if (std::hypot(b.x - cx, b.y - cy) < b.r) return "body overlaps a wall at start";
    }
  }
  for (const auto& w : s.walls) {
    if (!std::isfinite(w.x) || !std::isfinite(w.y) || !std::isfinite(w.w) || !std::isfinite(w.h))
      return "non-finite wall field";
    if (std::fmin(w.w, w.h) < L::wall_thickness_min) return "wall thinner than 0.1";
    if (w.x < 0 || w.y < 0 || w.x + w.w > s.arena_w || w.y + w.h > s.arena_h) return "wall outside the arena";
  }
  if (s.ctrl.enabled) {
    if (s.ctrl.body < 0 || s.ctrl.body >= static_cast<int>(s.bodies.size())) return "controller body index invalid";
    if (s.ctrl.waypoints.empty() || static_cast<int>(s.ctrl.waypoints.size()) > L::waypoints_max)
      return "controller needs 1 to 4 waypoints";
    if (!std::isfinite(s.ctrl.kp) || s.ctrl.kp <= 0 || s.ctrl.kp > 100) return "kp outside (0, 100]";
    if (!std::isfinite(s.ctrl.fmax) || s.ctrl.fmax < L::fmax_min || s.ctrl.fmax > L::fmax_max)
      return "fmax outside [5, 200]";
    if (!std::isfinite(s.ctrl.vcmd) || s.ctrl.vcmd <= 0 || s.ctrl.vcmd > 20) return "vcmd outside (0, 20]";
    if (!std::isfinite(s.ctrl.noise) || s.ctrl.noise < 0 || s.ctrl.noise > 1) return "noise outside [0, 1]";
    for (const auto& w : s.ctrl.waypoints)
      if (!std::isfinite(w.x) || !std::isfinite(w.y) || w.x < 0 || w.x > s.arena_w || w.y < 0 || w.y > s.arena_h)
        return "waypoint outside the arena";
  }
  return "";
}

uint64_t scenario_hash(const Scenario& s) {
  std::string d = to_json(s).dump();
  return fnv1a(d.data(), d.size());
}

int scenario_size(const Scenario& s) {
  return static_cast<int>(s.bodies.size()) + static_cast<int>(s.walls.size()) +
         static_cast<int>(s.ctrl.enabled ? 1 + s.ctrl.waypoints.size() : 0) + s.steps / 100;
}

}  // namespace rc
