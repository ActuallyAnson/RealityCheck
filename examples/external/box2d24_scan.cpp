// rc_scan generic-domain adapter around unmodified Box2D v2.4.1 (class API). Closed box with circles/boxes (optionally joined by revolute joints).
// params: balls, size, restitution, friction, density, gravity, speed, dt, viter, piter, bullet(0/1), sleep(0/1), shape(0 circles,1 boxes,2 mixed), joints(0/1), ccd(0/1)
// symmetry hooks for rc_scan --same: mirror (0/1) reflects the whole scene left to right, shift moves it along x; neither should change the result.
#include <box2d/box2d.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include "util/json.hpp"
using rc::Json;
static void wall(b2World& w, float cx, float cy, float hw, float hh) {
  b2BodyDef bd; bd.position.Set(cx, cy);
  b2Body* b = w.CreateBody(&bd);
  b2PolygonShape s; s.SetAsBox(hw, hh); b->CreateFixture(&s, 0.f);
}
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    Json out = Json::object();
    try {
      const Json req = Json::parse(line);
      const Json& sc = req.at("scenario");
      const Json& p = sc.at("params");
      const int steps = (int)sc.num_or("steps", 200);
      const int n = (int)p.at("balls").num();
      const float sz = (float)p.at("size").num(), e = (float)p.at("restitution").num(), mu = (float)p.at("friction").num(),
                  rho = (float)p.at("density").num(), g = (float)p.at("gravity").num(), speed = (float)p.at("speed").num(), dt = (float)p.at("dt").num();
      const int vi = (int)p.at("viter").num(), pi = (int)p.at("piter").num(), shape = (int)p.at("shape").num();
      const bool bullet = p.at("bullet").num() != 0, sleep = p.at("sleep").num() != 0, joints = p.at("joints").num() != 0, ccd = p.at("ccd").num() != 0;
      const bool mirror = p.num_or("mirror", 0) != 0; const float shift = (float)p.num_or("shift", 0);
      const float W = 20, H = 20, t = 1;
      b2World world(b2Vec2(0.f, -g));
      world.SetAllowSleeping(sleep);
      world.SetContinuousPhysics(ccd);
      wall(world, W / 2 + shift, -t, W / 2 + t, t); wall(world, W / 2 + shift, H + t, W / 2 + t, t);
      wall(world, -t + shift, H / 2, t, H / 2 + t); wall(world, W + t + shift, H / 2, t, H / 2 + t);
      std::vector<b2Body*> bodies; std::vector<float> ext;
      unsigned long long rng = (unsigned long long)sc.num_or("seed", 1) * 6364136223846793005ULL + 1442695040888963407ULL;
      auto u = [&] { rng = rng * 6364136223846793005ULL + 1442695040888963407ULL; return (float)(((rng >> 33) % 100000) / 100000.0); };
      for (int i = 0; i < n; ++i) {
        b2BodyDef bd; bd.type = b2_dynamicBody; bd.bullet = bullet;
        const float px = 2 + u() * (W - 4), py = 3 + u() * (H - 6);
        bd.position.Set((mirror ? W - px : px) + shift, py);
        const float a = u() * 6.2831853f;
        bd.linearVelocity.Set((mirror ? -1.f : 1.f) * speed * std::cos(a), speed * std::sin(a));
        const float ang = u() * 3.14f;
        bd.angle = mirror ? -ang : ang;
        b2Body* b = world.CreateBody(&bd);
        b2FixtureDef fd; fd.restitution = e; fd.friction = mu; fd.density = rho;
        bool box = shape == 1 || (shape == 2 && (i & 1));
        b2CircleShape cs; b2PolygonShape ps;
        if (box) { ps.SetAsBox(sz, sz * (0.5f + u())); fd.shape = &ps; } else { cs.m_radius = sz; fd.shape = &cs; }
        b->CreateFixture(&fd);
        bodies.push_back(b); ext.push_back(sz * 1.5f);
      }
      if (joints)
        for (int i = 0; i + 1 < n; i += 2) {
          b2RevoluteJointDef jd; jd.Initialize(bodies[i], bodies[i + 1], bodies[i]->GetPosition());
          jd.collideConnected = false; world.CreateJoint(&jd);
        }
      Json MS = Json::array(), MC = Json::array(), MX = Json::array(), KE = Json::array(), OUT = Json::array();
      auto snap = [&] {
        double ms = 0, mc = 1e9, mx = 0, ke = 0; int outn = 0;
        for (size_t i = 0; i < bodies.size(); ++i) {
          const b2Vec2 q = bodies[i]->GetPosition(), v = bodies[i]->GetLinearVelocity();
          ms = std::max(ms, (double)std::hypot(v.x, v.y));
          mc = std::min(mc, (double)q.y - ext[i]);
          mx = std::max(mx, (double)std::fabs(q.x - shift - W / 2));
          ke += 0.5 * bodies[i]->GetMass() * (v.x * v.x + v.y * v.y);
          if (q.x < shift || q.x > W + shift || q.y < 0 || q.y > H) ++outn;
        }
        if (bodies.empty()) mc = 0;
        MS.push(ms); MC.push(mc); MX.push(mx); KE.push(ke); OUT.push((double)outn);
      };
      snap();
      for (int k = 0; k < steps; ++k) { world.Step(dt, vi, pi); snap(); }
      Json sig = Json::object();
      sig["max_speed"] = MS; sig["min_clearance"] = MC; sig["max_abs_x"] = MX; sig["kinetic"] = KE; sig["n_outside"] = OUT;
      out["ok"] = true; out["signals"] = sig;
    } catch (const std::exception& ex) { out["ok"] = false; out["error"] = ex.what(); }
    std::cout << out.dump() << "\n" << std::flush;
  }
  return 0;
}
