// Generic-domain adapter (docs/GENERIC_DOMAIN.md) around an UNMODIFIED REBOUND 5.x checkout, to try rc_scan on a real N-body engine. A few bodies are placed
// at random from the scenario seed; REBOUND integrates them to the output times. It encodes no expected answer and no known bug.
// Parameters: bodies (int), mass_ratio, spread, speed, softening, dt, integrator (choice), layout (choice: random | equal_masses_zero_momentum).
// Signals per output step: energy, momentum, max_radius. REBOUND is GPLv3 and built outside this repository (scripts/build_rebound_example.sh objects).
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#define restrict __restrict__  // REBOUND's headers use the C99 keyword
extern "C" {
#include "rebound.h"
}
#include "util/json.hpp"

using rc::Json;

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    Json out = Json::object();
    try {
      const Json req = Json::parse(line);
      const Json& sc = req.at("scenario");
      const Json& p = sc.at("params");
      const int steps = static_cast<int>(sc.num_or("steps", 50));
      const int n = static_cast<int>(p.at("bodies").num());
      const double ratio = p.at("mass_ratio").num(), spread = p.at("spread").num(), speed = p.at("speed").num(), soft = p.at("softening").num(), dt = p.at("dt").num();
      const std::string integ = p.at("integrator").str();
      const bool sym = p.at("layout").str() == "equal_masses_zero_momentum";
      unsigned long long rng = static_cast<unsigned long long>(sc.num_or("seed", 1)) * 6364136223846793005ULL + 1442695040888963407ULL;
      auto u = [&] { rng = rng * 6364136223846793005ULL + 1442695040888963407ULL; return ((rng >> 33) % 100000) / 100000.0; };
      reb_simulation* r = reb_simulation_create();
      reb_simulation_set_integrator(r, integ.c_str());
      r->G = 1.0;
      r->softening = soft;
      r->dt = dt;
      r->exact_finish_time = 1;
      double px = 0, py = 0, pvx = 0, pvy = 0;
      for (int i = 0; i < n; ++i) {
        reb_particle q = {};
        q.m = sym ? 1.0 : 1.0 + (ratio - 1.0) * u();
        q.x = 10 + spread * (u() - 0.5); q.y = 6 + spread * (u() - 0.5);
        q.vx = speed * (u() - 0.5); q.vy = speed * (u() - 0.5);
        if (sym && i == n - 1) { q.vx = -pvx; q.vy = -pvy; }  // last body cancels the others' momentum (equal masses: velocity sum zero)
        pvx += q.vx; pvy += q.vy; px += q.x; py += q.y;
        reb_simulation_add(r, q);
      }
      Json E = Json::array(), M = Json::array(), R = Json::array();
      auto snap = [&] {
        double ke = 0, pe = 0, mx = 0, my = 0, mr = 0;
        for (int i = 0; i < r->N; ++i) {
          const reb_particle& a = r->particles[i];
          ke += 0.5 * a.m * (a.vx * a.vx + a.vy * a.vy); mx += a.m * a.vx; my += a.m * a.vy;
          mr = std::max(mr, std::hypot(a.x - 10, a.y - 6));
          for (int j = i + 1; j < r->N; ++j) {
            const reb_particle& b = r->particles[j];
            pe -= r->G * a.m * b.m / std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + soft * soft);
          }
        }
        E.push(ke + pe); M.push(std::hypot(mx, my)); R.push(mr);
      };
      snap();
      for (int k = 1; k <= steps; ++k) { reb_simulation_integrate(r, k * dt); snap(); }
      reb_simulation_free(r);
      Json sig = Json::object();
      sig["energy"] = E; sig["momentum"] = M; sig["max_radius"] = R;
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
