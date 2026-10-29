// Adapter for REBOUND 4.x-era checkouts (before the 5.0 API: integrators are chosen with the r->integrator enum), used to run the same orbit-domain
// scenarios on the commit before and the commit after a historical bug fix. Same protocol and same softened force law as rebound_orbit.cpp.
// Usage: ./rebound4_orbit <leapfrog|ias15|bs|whfast|mercurius|trace> [substeps_per_dt]
// REBOUND is GPLv3 and built outside this repository (scripts/build_rebound_hist.sh); nothing of it is copied here.
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#define restrict __restrict__  // REBOUND's headers use the C99 keyword
extern "C" {
#include "rebound.h"
}
#include "util/json.hpp"

using rc::Json;

int main(int argc, char** argv) {
  const std::string integ = argc > 1 ? argv[1] : "ias15";
  const int sub = argc > 2 ? std::max(1, std::atoi(argv[2])) : 1;
  std::string line;
  while (std::getline(std::cin, line)) {
    Json out = Json::object();
    try {
      Json req = Json::parse(line);
      const Json& sc = req.at("scenario");
      const double G = sc.num_or("G", 1), dt = sc.num_or("dt", 0.01);
      const int steps = static_cast<int>(sc.num_or("steps", 100));
      reb_simulation* r = reb_simulation_create();
      if (integ == "leapfrog") r->integrator = reb_simulation::REB_INTEGRATOR_LEAPFROG;
      else if (integ == "ias15") r->integrator = reb_simulation::REB_INTEGRATOR_IAS15;
#ifndef RB_NO_BS
      else if (integ == "bs") r->integrator = reb_simulation::REB_INTEGRATOR_BS;
#endif
      else if (integ == "whfast") r->integrator = reb_simulation::REB_INTEGRATOR_WHFAST;
      else if (integ == "mercurius") r->integrator = reb_simulation::REB_INTEGRATOR_MERCURIUS;
#ifndef RB_NO_TRACE
      else if (integ == "trace") r->integrator = reb_simulation::REB_INTEGRATOR_TRACE;
#endif
      else throw std::runtime_error("unknown integrator " + integ);
      r->G = G;
      r->softening = 0.3;
      r->dt = dt / sub;
      r->exact_finish_time = 1;
      for (const auto& o : sc.at("bodies").arr()) {
        reb_particle p = {};
        p.x = o.num_or("x", 0); p.y = o.num_or("y", 0); p.vx = o.num_or("vx", 0); p.vy = o.num_or("vy", 0); p.m = o.num_or("m", 1);
        reb_simulation_add(r, p);
      }
      Json frames = Json::array();
      auto snap = [&] {
        Json f = Json::array();
        for (int i = 0; i < r->N; ++i) { const reb_particle& q = r->particles[i]; f.push(q.x); f.push(q.y); f.push(q.vx); f.push(q.vy); }
        frames.push(f);
      };
      snap();
      for (int k = 1; k <= steps; ++k) { reb_simulation_integrate(r, k * dt); snap(); }
      reb_simulation_free(r);
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
