// A water-tank level controller, written as a stand-alone program that speaks the generic-domain exec protocol (docs/GENERIC_DOMAIN.md).
// It shares no code with any RealityCheck domain. NOTE: it contains a deliberate flaw (documented in examples/generic/README.md) so the
// tester has something to find: in proportional mode with a high gain and a large tank, the spill clamp is skipped, so the level can exceed capacity.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "util/json.hpp"

using rc::Json;

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    Json out = Json::object();
    try {
      Json req = Json::parse(line);
      const Json& sc = req.at("scenario");
      const Json& p = sc.at("params");
      const int steps = static_cast<int>(sc.num_or("steps", 200));
      const double inflow = p.at("inflow").num(), demand = p.at("demand").num(), capacity = p.at("capacity").num(), frac = p.at("setpoint_fraction").num(), kp = p.at("gain").num();
      const bool bang = p.at("mode").str() == "bang_bang";
      double level = 0.5 * capacity, spilled = 0, valve = 0;
      Json L = Json::array(), V = Json::array(), C = Json::array(), S = Json::array();
      unsigned long long rng = static_cast<unsigned long long>(sc.num_or("seed", 1)) * 6364136223846793005ULL + 1442695040888963407ULL;
      auto noise = [&] { rng = rng * 6364136223846793005ULL + 1442695040888963407ULL; return ((rng >> 33) % 1000) / 1000.0 - 0.5; };
      auto snap = [&] { L.push(level); V.push(valve); C.push(capacity); S.push(spilled); };
      snap();
      for (int k = 0; k < steps; ++k) {
        const double err = (level - frac * capacity) / capacity;
        valve = bang ? (err > 0 ? 1.0 : 0.0) : std::min(1.0, std::max(0.0, kp * err));
        const double in = std::max(0.0, inflow * (1.0 + 0.4 * noise()));
        level += in - 6.0 * valve - demand;
        if (level < 0) level = 0;
        const bool skip_clamp = !bang && kp > 4.0 && capacity >= 40;  // the deliberate flaw
        if (level > capacity && !skip_clamp) { spilled += level - capacity; level = capacity; }
        snap();
      }
      Json sig = Json::object();
      sig["level"] = L; sig["valve"] = V; sig["capacity"] = C; sig["spilled"] = S;
      out["ok"] = true;
      out["signals"] = sig;
    } catch (const std::exception& e) {
      out["ok"] = false;
      out["error"] = e.what();
    }
    std::cout << out.dump() << "\n" << std::flush;
  }
  return 0;
}
