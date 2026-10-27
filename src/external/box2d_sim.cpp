#include "external/box2d_sim.hpp"

#include <box2d/box2d.h>

#include <cmath>

namespace rc {

namespace {

struct WorldGuard {
  b2WorldId id;
  explicit WorldGuard(b2WorldId w) : id(w) {}
  ~WorldGuard() { b2DestroyWorld(id); }
};

void add_box(b2WorldId w, double cx, double cy, double hw, double hh, float restitution) {
  b2BodyDef bd = b2DefaultBodyDef();
  bd.type = b2_staticBody;
  bd.position = {static_cast<float>(cx), static_cast<float>(cy)};
  b2BodyId body = b2CreateBody(w, &bd);
  b2ShapeDef sd = b2DefaultShapeDef();
  sd.material.restitution = restitution;
  b2Polygon box = b2MakeBox(static_cast<float>(hw), static_cast<float>(hh));
  b2CreatePolygonShape(body, &sd, &box);
}

}  // namespace

SimFn box2d_sim(Box2DOptions opt) {
  return [opt](const Scenario& sc) {
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = {0.0f, -static_cast<float>(sc.gravity)};
    wd.enableContinuous = opt.continuous;
    WorldGuard world(b2CreateWorld(&wd));

    // Arena: four slabs just outside [0,arena_w] x [0,arena_h]. Wall restitution 0 so the body's own
    // restitution decides the bounce (Box2D mixes with max()).
    const double t = 1.0;
    add_box(world.id, sc.arena_w / 2, -t, sc.arena_w / 2 + t, t, 0.f);
    add_box(world.id, sc.arena_w / 2, sc.arena_h + t, sc.arena_w / 2 + t, t, 0.f);
    add_box(world.id, -t, sc.arena_h / 2, t, sc.arena_h / 2 + t, 0.f);
    add_box(world.id, sc.arena_w + t, sc.arena_h / 2, t, sc.arena_h / 2 + t, 0.f);
    for (const WallDef& w : sc.walls) add_box(world.id, w.x + w.w / 2, w.y + w.h / 2, w.w / 2, w.h / 2, 0.f);

    std::vector<b2BodyId> bodies;
    for (const BodyDef& d : sc.bodies) {
      b2BodyDef bd = b2DefaultBodyDef();
      bd.type = b2_dynamicBody;
      bd.position = {static_cast<float>(d.x), static_cast<float>(d.y)};
      bd.linearVelocity = {static_cast<float>(d.vx), static_cast<float>(d.vy)};
      b2BodyId body = b2CreateBody(world.id, &bd);
      b2ShapeDef sd = b2DefaultShapeDef();
      sd.density = static_cast<float>(d.m / (M_PI * d.r * d.r));
      sd.material.restitution = static_cast<float>(d.e);
      b2Circle c = {{0.f, 0.f}, static_cast<float>(d.r)};
      b2CreateCircleShape(body, &sd, &c);
      bodies.push_back(body);
    }

    Trace tr;
    auto record = [&] {
      StepRecord r;
      double e = 0;
      for (size_t i = 0; i < bodies.size(); ++i) {
        b2Vec2 p = b2Body_GetPosition(bodies[i]);
        b2Vec2 v = b2Body_GetLinearVelocity(bodies[i]);
        BodyState q;
        q.x = p.x; q.y = p.y; q.vx = v.x; q.vy = v.y;
        r.body.push_back(q);
        double m = sc.bodies[i].m;
        e += 0.5 * m * (q.vx * q.vx + q.vy * q.vy) + m * sc.gravity * q.y;
      }
      r.energy = e;
      r.substeps = opt.substeps;
      tr.rec.push_back(std::move(r));
    };
    record();
    for (int k = 0; k < sc.steps; ++k) {
      b2World_Step(world.id, static_cast<float>(sc.dt), opt.substeps);
      record();
    }
    return tr;
  };
}

}  // namespace rc
