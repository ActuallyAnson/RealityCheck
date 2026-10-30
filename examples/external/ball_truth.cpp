// Analytic truth for a ball dropped onto the floor (Box2D historical-fix test, docs/REAL_PROJECTS.md). A frictionless circle of radius r falls from rest under
// gravity g onto a floor whose top is y=0 and bounces with restitution e; a bounce happens only if the impact speed exceeds the engine's restitution threshold
// (Box2D default 1 m/s), otherwise the ball rests. No wall or other body is touched (vx=0, mid-arena).
// ./ball_truth gen SEED N          one pipe-protocol rigid2d request per line (fixed horizon: 2 bounces + 0.3 s, as the truth predicts)
// ./ball_truth score REQ A.out B.out   per scenario RMS error of the centre height against the truth for A and B, and a two-sided exact sign test
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "util/json.hpp"

using rc::Json;

static double truth_y(double y0, double r, double g, double e, double t, double thr) {
  double h = y0 - r, v = 0.0, now = 0.0;  // height of the centre above its resting height r, downward-positive speed v
  for (int bounce = 0; bounce < 40; ++bounce) {
    const double tf = (-v + std::sqrt(v * v + 2 * g * h)) / g;  // time to reach the floor from the current state
    if (t <= now + tf) { const double s = t - now; return r + h - v * s - 0.5 * g * s * s; }
    now += tf;
    const double vi2 = std::sqrt(v * v + 2 * g * h);  // impact speed
    if (vi2 <= thr) return r;
    h = 0; v = -e * vi2;  // upward launch speed e*vi2 (v is downward-positive)
  }
  return r;
}

static double sign_test(int w, int l) {
  const int n = w + l, m = std::min(w, l);
  double c = 1, s = 0;
  for (int k = 0; k <= m; ++k) { s += c; c = c * (n - k) / (k + 1); }
  return n ? std::min(1.0, 2 * s / std::pow(2.0, n)) : 1.0;
}

int main(int argc, char** argv) {
  const std::string mode = argc > 1 ? argv[1] : "";
  const double g = 9.81, dt = 1.0 / 60, thr = 1.0;
  if (mode == "gen") {
    std::mt19937_64 rng(static_cast<unsigned>(std::atoi(argc > 2 ? argv[2] : "1")));
    const int n = argc > 3 ? std::atoi(argv[3]) : 1000;
    auto u = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
    for (int i = 0; i < n; ++i) {
      const double r = u(0.2, 0.6), y0 = u(3, 10), e = u(0.3, 0.95);
      const double t1 = std::sqrt(2 * (y0 - r) / g), v1 = g * t1, t2 = t1 + 2 * e * v1 / g;
      Json body = Json::object();
      body["x"] = 10.0; body["y"] = y0; body["vx"] = 0.0; body["vy"] = 0.0; body["r"] = r; body["m"] = 1.0; body["e"] = e;
      Json bodies = Json::array(); bodies.push(body);
      Json arena = Json::object(); arena["w"] = 20.0; arena["h"] = 12.0;
      Json sc = Json::object();
      sc["arena"] = arena; sc["bodies"] = bodies; sc["dt"] = dt; sc["gravity"] = g; sc["steps"] = static_cast<int>((t2 + 0.3) / dt) + 1;
      sc["walls"] = Json::array(); sc["seed"] = i;
      Json req = Json::object(); req["op"] = "simulate"; req["scenario"] = sc;
      std::cout << req.dump() << "\n";
    }
    return 0;
  }
  if (mode == "score" && argc > 4) {
    std::ifstream fr(argv[2]), fa(argv[3]), fb(argv[4]);
    std::string lr, la, lb;
    int w = 0, l = 0, tie = 0, n = 0, dropped = 0, differ = 0;
    std::vector<double> ea, eb;
    while (std::getline(fr, lr) && std::getline(fa, la) && std::getline(fb, lb)) {
      const Json rq = Json::parse(lr), a = Json::parse(la), b = Json::parse(lb);
      if (!a.at("ok").boolean() || !b.at("ok").boolean()) { ++dropped; continue; }
      const Json& body = rq.at("scenario").at("bodies").arr().at(0);
      const double r = body.num_or("r", 0.5), y0 = body.num_or("y", 0), e = body.num_or("e", 0.8);
      double sa = 0, sb = 0; bool diff = false;
      const auto& fra = a.at("frames").arr(); const auto& frb = b.at("frames").arr();
      for (size_t k = 0; k < fra.size(); ++k) {
        const double t = static_cast<double>(k) * dt, ty = truth_y(y0, r, g, e, t, thr);
        const double ya = fra[k].arr().at(1).num(), yb = frb[k].arr().at(1).num();
        sa += (ya - ty) * (ya - ty); sb += (yb - ty) * (yb - ty); diff = diff || ya != yb;
      }
      ++n; sa = std::sqrt(sa / static_cast<double>(fra.size())); sb = std::sqrt(sb / static_cast<double>(fra.size()));
      ea.push_back(sa); eb.push_back(sb);
      if (!diff) continue;
      ++differ;
      if (sb < sa) ++w; else if (sb > sa) ++l; else ++tie;
    }
    std::sort(ea.begin(), ea.end()); std::sort(eb.begin(), eb.end());
    std::printf("scenarios run by both builds %d (dropped %d); builds differ in %d; B closer to truth %d, A closer %d, ties %d; two-sided sign test p = %.2g; median RMS height error A %.4f, B %.4f\n",
                n, dropped, differ, w, l, tie, sign_test(w, l), ea.empty() ? 0.0 : ea[ea.size() / 2], eb.empty() ? 0.0 : eb[eb.size() / 2]);
    return 0;
  }
  return 2;
}
