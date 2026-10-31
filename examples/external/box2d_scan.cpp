// Generic-domain adapter (docs/GENERIC_DOMAIN.md) around an UNMODIFIED Box2D v3 checkout, made to try rc_scan on a real third-party engine: a pile of circles
// dropped into a closed box. It encodes no expected answer and no known bug. Box2D is MIT and built outside this repository (scripts/build_box2d_hist.sh).
// Parameters: balls (int), radius, restitution, friction, gravity, speed (initial speed), dt, substeps (int), continuous (0/1).
// Signals per step: max_speed, min_clearance (lowest ball bottom above the floor), max_abs_x (furthest ball centre from the box middle), kinetic.
#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "util/json.hpp"

using rc::Json;

static void add_box(b2WorldId w, double cx, double cy, double hw, double hh) {
  b2BodyDef bd = b2DefaultBodyDef();
  bd.position = {static_cast<float>(cx), static_cast<float>(cy)};
  b2BodyId body = b2CreateBody(w, &bd);
  b2ShapeDef sd = b2DefaultShapeDef();
  b2Polygon box = b2MakeBox(static_cast<float>(hw), static_cast<float>(hh));
  b2CreatePolygonShape(body, &sd, &box);
}

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    Json out = Json::object();
    try {
      const Json req = Json::parse(line);
      const Json& sc = req.at("scenario");
      const Json& p = sc.at("params");
      const int steps = static_cast<int>(sc.num_or("steps", 200));
      const int n = static_cast<int>(p.at("balls").num());
      const double r = p.at("radius").num(), e = p.at("restitution").num(), mu = p.at("friction").num(), g = p.at("gravity").num(), speed = p.at("speed").num();
      const double dt = p.at("dt").num();
      const int sub = static_cast<int>(p.at("substeps").num());
      const bool cont = p.at("continuous").num() != 0;
      const double W = 20, H = 20, t = 1;
      b2WorldDef wd = b2DefaultWorldDef();
      wd.gravity = {0.f, -static_cast<float>(g)};
      wd.enableContinuous = cont;
      b2WorldId world = b2CreateWorld(&wd);
      add_box(world, W / 2, -t, W / 2 + t, t);
      add_box(world, W / 2, H + t, W / 2 + t, t);
      add_box(world, -t, H / 2, t, H / 2 + t);
      add_box(world, W + t, H / 2, t, H / 2 + t);
      std::vector<b2BodyId> bodies;
      unsigned long long rng = static_cast<unsigned long long>(sc.num_or("seed", 1)) * 6364136223846793005ULL + 1442695040888963407ULL;
      auto u = [&] { rng = rng * 6364136223846793005ULL + 1442695040888963407ULL; return ((rng >> 33) % 100000) / 100000.0; };
      for (int i = 0; i < n; ++i) {
        b2BodyDef bd = b2DefaultBodyDef();
        bd.type = b2_dynamicBody;
        bd.position = {static_cast<float>(2 + u() * (W - 4)), static_cast<float>(3 + u() * (H - 6))};
        const double a = u() * 6.283185307179586;
        bd.linearVelocity = {static_cast<float>(speed * std::cos(a)), static_cast<float>(speed * std::sin(a))};
        b2BodyId body = b2CreateBody(world, &bd);
        b2ShapeDef sd = b2DefaultShapeDef();
        sd.material.restitution = static_cast<float>(e);
        sd.material.friction = static_cast<float>(mu);
        b2Circle c = {{0.f, 0.f}, static_cast<float>(r)};
        b2CreateCircleShape(body, &sd, &c);
        bodies.push_back(body);
      }
      Json MS = Json::array(), MC = Json::array(), MX = Json::array(), KE = Json::array();
      auto snap = [&] {
        double ms = 0, mc = 1e9, mx = 0, ke = 0;
        for (b2BodyId b : bodies) {
          const b2Vec2 q = b2Body_GetPosition(b), v = b2Body_GetLinearVelocity(b);
          ms = std::max(ms, static_cast<double>(std::hypot(v.x, v.y)));
          mc = std::min(mc, static_cast<double>(q.y) - r);
          mx = std::max(mx, static_cast<double>(std::fabs(q.x - W / 2)));
          ke += 0.5 * (v.x * v.x + v.y * v.y);
        }
        MS.push(ms); MC.push(mc); MX.push(mx); KE.push(ke);
      };
      snap();
      for (int k = 0; k < steps; ++k) { b2World_Step(world, static_cast<float>(dt), sub); snap(); }
      b2DestroyWorld(world);
      Json sig = Json::object();
      sig["max_speed"] = MS; sig["min_clearance"] = MC; sig["max_abs_x"] = MX; sig["kinetic"] = KE;
      out["ok"] = true;
      out["signals"] = sig;
    } catch (const std::exception& ex) {
      out["ok"] = false;
      out["error"] = ex.what();
    }
    std::cout << out.dump() << "\n" << std::flush;
  }
  return 0;
}
