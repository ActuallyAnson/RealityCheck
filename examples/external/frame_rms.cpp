// Compares two JSON-lines outputs of the pipe protocol (one response per scenario, same order): RMS position error over all bodies at one frame,
// per scenario, and the median over scenarios. Usage: ./frame_rms A.out B.out [frame=100]
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "util/json.hpp"

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  const size_t k = argc > 3 ? static_cast<size_t>(std::atoi(argv[3])) : 100;
  std::ifstream fa(argv[1]), fb(argv[2]);
  std::string la, lb;
  std::vector<double> e;
  while (std::getline(fa, la) && std::getline(fb, lb)) {
    const rc::Json a = rc::Json::parse(la), b = rc::Json::parse(lb);
    if (!a.at("ok").boolean() || !b.at("ok").boolean()) { std::cout << "nan\n"; continue; }
    const auto& xa = a.at("frames").arr().at(k).arr();
    const auto& xb = b.at("frames").arr().at(k).arr();
    const size_t n = xa.size() / 4;
    double s = 0;
    for (size_t i = 0; i < n; ++i) {
      const double dx = xa[4 * i].num() - xb[4 * i].num(), dy = xa[4 * i + 1].num() - xb[4 * i + 1].num();
      s += dx * dx + dy * dy;
    }
    e.push_back(std::sqrt(s / static_cast<double>(n)));
    std::cout << e.back() << "\n";
  }
  std::sort(e.begin(), e.end());
  std::cerr << "scenarios " << e.size() << " median " << (e.empty() ? 0.0 : e[e.size() / 2]) << "\n";
}
