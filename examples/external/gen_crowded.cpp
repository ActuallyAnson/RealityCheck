// Scenario generator for the REBOUND TRACE truth test (docs/REAL_PROJECTS.md): a star (m=100) and 3-5 giant planets (m 0.5-3) crowded at r 3-3.6 with speed
// 0.8-1.1 of circular, dt 0.05, 1500 steps; one pipe-protocol request per line. Usage: ./gen_crowded SEED COUNT > req.jsonl
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>

#include "util/json.hpp"

using rc::Json;

int main(int argc, char** argv) {
  const unsigned seed = argc > 1 ? static_cast<unsigned>(std::atoi(argv[1])) : 12;
  const int count = argc > 2 ? std::atoi(argv[2]) : 600;
  std::mt19937_64 rng(seed);
  auto u = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
  for (int s = 0; s < count; ++s) {
    Json bodies = Json::array();
    Json star = Json::object();
    star["x"] = 0.0; star["y"] = 0.0; star["vx"] = 0.0; star["vy"] = 0.0; star["m"] = 100.0;
    bodies.push(star);
    const int n = 3 + static_cast<int>(rng() % 3);
    for (int i = 0; i < n; ++i) {
      const double r = u(3, 3.6), th = u(0, 2 * M_PI), v = std::sqrt(100.0 / r) * u(0.8, 1.1);
      Json b = Json::object();
      b["x"] = r * std::cos(th); b["y"] = r * std::sin(th); b["vx"] = -v * std::sin(th); b["vy"] = v * std::cos(th); b["m"] = u(0.5, 3.0);
      bodies.push(b);
    }
    Json sc = Json::object();
    sc["domain"] = "orbit"; sc["G"] = 1.0; sc["dt"] = 0.05; sc["steps"] = 1500; sc["bodies"] = bodies;
    Json req = Json::object();
    req["op"] = "simulate"; req["scenario"] = sc;
    std::cout << req.dump() << "\n";
  }
}
