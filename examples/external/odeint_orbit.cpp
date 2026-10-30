// Orbit simulator on top of Boost.odeint (Boost Software License 1.0), speaking the JSON-lines protocol (docs/PROTOCOL.md). The library code is unmodified;
// only the right-hand side (softened gravity, same law as the orbit domain) and the protocol loop are ours.
// Usage: ./odeint_orbit [stepper] [h_or_tolerance]   stepper: rk4 (fixed substep h, default 0.004) | dopri5 (adaptive, abs/rel tolerance, default 1e-6) | bs (Bulirsch-Stoer, adaptive, abs/rel tolerance, default 1e-6)
#include <boost/numeric/odeint.hpp>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "util/json.hpp"

using rc::Json;
namespace odeint = boost::numeric::odeint;
using state_t = std::vector<double>;  // x0,y0,... then vx0,vy0,...

int main(int argc, char** argv) {
  const std::string stepper = argc > 1 ? argv[1] : "rk4";
  const double param = argc > 2 ? std::atof(argv[2]) : (stepper == "dopri5" || stepper == "bs" ? 1e-6 : 0.004);
  const double soft = std::getenv("RC_SOFTENING") ? std::atof(std::getenv("RC_SOFTENING")) : 0.3, eps2 = soft * soft;
  std::string line;
  while (std::getline(std::cin, line)) {
    Json out = Json::object();
    try {
      Json req = Json::parse(line);
      const Json& sc = req.at("scenario");
      const double G = sc.num_or("G", 1), dt = sc.num_or("dt", 0.01);
      const int steps = static_cast<int>(sc.num_or("steps", 100));
      std::vector<double> m;
      state_t s;
      for (const auto& o : sc.at("bodies").arr()) { m.push_back(o.num_or("m", 1)); }
      const size_t n = m.size();
      s.assign(4 * n, 0);
      size_t i = 0;
      for (const auto& o : sc.at("bodies").arr()) { s[2 * i] = o.num_or("x", 0); s[2 * i + 1] = o.num_or("y", 0); s[2 * n + 2 * i] = o.num_or("vx", 0); s[2 * n + 2 * i + 1] = o.num_or("vy", 0); ++i; }
      auto accel = [&](const state_t& q, state_t& a) {  // q: positions (2n), a: accelerations (2n)
        std::fill(a.begin(), a.end(), 0.0);
        for (size_t p = 0; p < n; ++p) for (size_t r = 0; r < n; ++r) {
          if (p == r) continue;
          double dx = q[2 * r] - q[2 * p], dy = q[2 * r + 1] - q[2 * p + 1], r2 = dx * dx + dy * dy + eps2, f = G * m[r] / (r2 * std::sqrt(r2));
          a[2 * p] += f * dx; a[2 * p + 1] += f * dy;
        }
      };
      auto rhs = [&](const state_t& y, state_t& dydt, double) {
        state_t q(y.begin(), y.begin() + 2 * n), a(2 * n);
        accel(q, a);
        for (size_t k = 0; k < 2 * n; ++k) { dydt[k] = y[2 * n + k]; dydt[2 * n + k] = a[k]; }
      };
      Json frames = Json::array();
      auto snap = [&] { Json f = Json::array(); for (size_t p = 0; p < n; ++p) { f.push(s[2 * p]); f.push(s[2 * p + 1]); f.push(s[2 * n + 2 * p]); f.push(s[2 * n + 2 * p + 1]); } frames.push(f); };
      snap();
      for (int k = 0; k < steps; ++k) {
        const double t0 = k * dt;
        if (stepper == "dopri5") odeint::integrate_adaptive(odeint::make_controlled<odeint::runge_kutta_dopri5<state_t>>(param, param), rhs, s, t0, t0 + dt, dt / 10);
        else if (stepper == "bs") odeint::integrate_adaptive(odeint::bulirsch_stoer<state_t>(param, param), rhs, s, t0, t0 + dt, dt / 10);
        else {
          const int sub = std::max(1, static_cast<int>(std::ceil(dt / param)));
          odeint::integrate_const(odeint::runge_kutta4<state_t>(), rhs, s, t0, t0 + dt, dt / sub);
        }
        snap();
      }
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
