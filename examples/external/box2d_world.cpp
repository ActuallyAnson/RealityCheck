// Pipe-protocol adapter (docs/PROTOCOL.md, rigid2d domain) around an UNMODIFIED Box2D v3 checkout, so that the commit before and the commit after a
// historical Box2D bug fix can be run on the same scenarios. Same world construction as src/external/box2d_sim.cpp (Box2D itself is MIT, built outside this
// repository by scripts/build_box2d_hist.sh). Usage: ./box2d_world [substeps=4] [continuous=1]
#include <box2d/box2d.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "util/json.hpp"

using rc::Json;

static void add_box(b2WorldId w, double cx, double cy, double hw, double hh) {
  b2BodyDef bd = b2DefaultBodyDef();
  bd.type = b2_staticBody;
  bd.position = {static_cast<float>(cx), static_cast<float>(cy)};
  b2BodyId body = b2CreateBody(w, &bd);
  b2ShapeDef sd = b2DefaultShapeDef();
  sd.material.restitution = 0.f;
  b2Polygon box = b2MakeBox(static_cast<float>(hw), static_cast<float>(hh));
  b2CreatePolygonShape(body, &sd, &box);
}

int main(int argc, char** argv) {
  const int substeps = argc > 1 ? std::max(1, std::atoi(argv[1])) : 4;
  const bool continuous = argc > 2 ? std::atoi(argv[2]) != 0 : true;
  std::string line;
  while (std::getline(std::cin, line)) {
    Json out = Json::object();
    try {
      const Json req = Json::parse(line);
      const Json& sc = req.at("scenario");
      const double dt = sc.num_or("dt", 1.0 / 60), g = sc.num_or("gravity", 9.81);
      const int steps = static_cast<int>(sc.num_or("steps", 100));
      const double aw = sc.at("arena").num_or("w", 20), ah = sc.at("arena").num_or("h", 12);
      b2WorldDef wd = b2DefaultWorldDef();
      wd.gravity = {0.0f, -static_cast<float>(g)};
      wd.enableContinuous = continuous;
      b2WorldId world = b2CreateWorld(&wd);
      const double t = 1.0;
      add_box(world, aw / 2, -t, aw / 2 + t, t);
      add_box(world, aw / 2, ah + t, aw / 2 + t, t);
      add_box(world, -t, ah / 2, t, ah / 2 + t);
      add_box(world, aw + t, ah / 2, t, ah / 2 + t);
      if (sc.has("walls"))
        for (const auto& w : sc.at("walls").arr()) {
          const double x = w.num_or("x", 0), y = w.num_or("y", 0), ww = w.num_or("w", 1), hh = w.num_or("h", 1);
          add_box(world, x + ww / 2, y + hh / 2, ww / 2, hh / 2);
        }
      std::vector<b2BodyId> bodies;
      for (const auto& d : sc.at("bodies").arr()) {
        const double r = d.num_or("r", 0.5), m = d.num_or("m", 1.0);
        b2BodyDef bd = b2DefaultBodyDef();
        bd.type = b2_dynamicBody;
        bd.position = {static_cast<float>(d.num_or("x", 0)), static_cast<float>(d.num_or("y", 0))};
        bd.linearVelocity = {static_cast<float>(d.num_or("vx", 0)), static_cast<float>(d.num_or("vy", 0))};
        b2BodyId body = b2CreateBody(world, &bd);
        b2ShapeDef sd = b2DefaultShapeDef();
        sd.density = static_cast<float>(m / (M_PI * r * r));
        sd.material.restitution = static_cast<float>(d.num_or("e", 0.8));
        b2Circle c = {{0.f, 0.f}, static_cast<float>(r)};
        b2CreateCircleShape(body, &sd, &c);
        bodies.push_back(body);
      }
      Json frames = Json::array();
      auto snap = [&] {
        Json f = Json::array();
        for (b2BodyId b : bodies) {
          b2Vec2 p = b2Body_GetPosition(b), v = b2Body_GetLinearVelocity(b);
          f.push(p.x); f.push(p.y); f.push(v.x); f.push(v.y);
        }
        frames.push(f);
      };
      snap();
      for (int k = 0; k < steps; ++k) { b2World_Step(world, static_cast<float>(dt), substeps); snap(); }
      b2DestroyWorld(world);
      out["ok"] = true;
      out["frames"] = frames;
    } catch (const std::exception& e) {
      out["ok"] = false;
      out["error"] = e.what();
    }
    std::cout << out.dump() << "\n" << std::flush;
  }
  return 0;
}
