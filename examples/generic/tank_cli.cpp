// The water-tank example as a plain command-line program: no JSON, no rc code. Prints a table (header of signal names, one row per step) so that
//   rc_scan --cmd "./tank_cli --inflow {inflow} --demand {demand} --capacity {capacity} --frac {setpoint_fraction} --gain {gain} --mode {mode} --steps {steps} --seed {seed}" ...
// needs no adapter. It contains the same deliberate flaw as tank.cpp (proportional mode, high gain, large tank: the spill clamp is skipped).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
  double inflow = 4, demand = 1, capacity = 30, frac = 0.5, gain = 1;
  std::string mode = "proportional";
  int steps = 200;
  unsigned long long seed = 1;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string k = argv[i];
    if (k == "--inflow") inflow = std::atof(argv[i + 1]); else if (k == "--demand") demand = std::atof(argv[i + 1]);
    else if (k == "--capacity") capacity = std::atof(argv[i + 1]); else if (k == "--frac") frac = std::atof(argv[i + 1]);
    else if (k == "--gain") gain = std::atof(argv[i + 1]); else if (k == "--mode") mode = argv[i + 1];
    else if (k == "--steps") steps = std::atoi(argv[i + 1]); else if (k == "--seed") seed = std::strtoull(argv[i + 1], nullptr, 10);
  }
  const bool bang = mode == "bang_bang";
  double level = 0.5 * capacity, spilled = 0, valve = 0;
  unsigned long long rng = seed * 6364136223846793005ULL + 1442695040888963407ULL;
  auto noise = [&] { rng = rng * 6364136223846793005ULL + 1442695040888963407ULL; return ((rng >> 33) % 1000) / 1000.0 - 0.5; };
  std::puts("level valve capacity spilled");
  std::printf("%g %g %g %g\n", level, valve, capacity, spilled);
  for (int k = 0; k < steps; ++k) {
    const double err = (level - frac * capacity) / capacity;
    valve = bang ? (err > 0 ? 1.0 : 0.0) : std::min(1.0, std::max(0.0, gain * err));
    const double in = std::max(0.0, inflow * (1.0 + 0.4 * noise()));
    level += in - 6.0 * valve - demand;
    if (level < 0) level = 0;
    const bool skip_clamp = !bang && gain > 4.0 && capacity >= 40;
    if (level > capacity && !skip_clamp) { spilled += level - capacity; level = capacity; }
    std::printf("%g %g %g %g\n", level, valve, capacity, spilled);
  }
  return 0;
}
