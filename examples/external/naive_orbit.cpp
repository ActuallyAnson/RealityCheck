// Example external simulator for the RealityCheck JSON-lines protocol (docs/PROTOCOL.md): a deliberately simple,
// independent orbit integrator with a FIXED substep (velocity Verlet). It shares no code with the built-in orbit
// simulator. Usage: ./naive_orbit [substep_seconds]   (default 0.004)
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "util/json.hpp"

using rc::Json;

int main(int argc, char** argv) {
  const double max_h = argc > 1 ? std::atof(argv[1]) : 0.004;
  const double eps2 = 0.3 * 0.3;
  std::string line;
  while (std::getline(std::cin, line)) {
    Json out = Json::object();
    try {
      Json req = Json::parse(line);
      const Json& sc = req.at("scenario");
      const double G = sc.num_or("G", 1), dt = sc.num_or("dt", 0.01);
      const int steps = static_cast<int>(sc.num_or("steps", 100));
      struct B { double x, y, vx, vy, m; };
      std::vector<B> b;
      for (const auto& o : sc.at("bodies").arr()) b.push_back({o.num_or("x", 0), o.num_or("y", 0), o.num_or("vx", 0), o.num_or("vy", 0), o.num_or("m", 1)});
      auto acc = [&](std::vector<double>& ax, std::vector<double>& ay) {
        ax.assign(b.size(), 0); ay.assign(b.size(), 0);
        for (size_t i = 0; i < b.size(); ++i)
          for (size_t j = 0; j < b.size(); ++j) {
            if (i == j) continue;
            double dx = b[j].x - b[i].x, dy = b[j].y - b[i].y, r2 = dx * dx + dy * dy + eps2;
            double f = G * b[j].m / (r2 * std::sqrt(r2));
            ax[i] += f * dx; ay[i] += f * dy;
          }
      };
      const int n = std::max(1, static_cast<int>(std::ceil(dt / max_h)));
      const double h = dt / n;
      Json frames = Json::array();
      auto snap = [&] {
        Json f = Json::array();
        for (const auto& q : b) { f.push(q.x); f.push(q.y); f.push(q.vx); f.push(q.vy); }
        frames.push(f);
      };
      snap();
      std::vector<double> ax, ay;
      for (int k = 0; k < steps; ++k) {
        for (int s = 0; s < n; ++s) {
          acc(ax, ay);
          for (size_t i = 0; i < b.size(); ++i) { b[i].vx += 0.5 * h * ax[i]; b[i].vy += 0.5 * h * ay[i]; b[i].x += h * b[i].vx; b[i].y += h * b[i].vy; }
          acc(ax, ay);
          for (size_t i = 0; i < b.size(); ++i) { b[i].vx += 0.5 * h * ax[i]; b[i].vy += 0.5 * h * ay[i]; }
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
