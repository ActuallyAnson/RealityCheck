// refrun: runs a model-written one-step reference (compiled in with -DREF_ORBIT or -DREF_CARTPOLE and -DREF_IMPL='"file.cpp"') over dump lines on stdin (from stepdump).
// It prints the reference's prediction for each line, once with one step of dt and once as two steps of dt/2 (the difference estimates the reference's own local error).
// It never sees the recorded next state. Meant to be run inside the sandbox profile (no network, no file writes, no forking).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef REF_ORBIT
#include "eval/ref_orbit_decl.hpp"
#else
#include "eval/ref_cartpole_decl.hpp"
#endif
#include REF_IMPL

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream is(line);
    long scen, k;
    is >> scen >> k;
#ifdef REF_ORBIT
    OrbitIn in{}; int n;
    is >> in.dt >> in.G >> n;
    in.n = n;
    for (int i = 0; i < n; ++i) is >> in.m[i];
    for (int i = 0; i < n; ++i) is >> in.x[i] >> in.y[i] >> in.vx[i] >> in.vy[i];
    OrbitOut full{}, h1{}, h2{};
    ref_step_orbit(in, &full);
    OrbitIn half = in; half.dt = in.dt / 2;
    ref_step_orbit(half, &h1);
    OrbitIn second = half;
    for (int i = 0; i < n; ++i) { second.x[i] = h1.x[i]; second.y[i] = h1.y[i]; second.vx[i] = h1.vx[i]; second.vy[i] = h1.vy[i]; }
    ref_step_orbit(second, &h2);
    std::printf("%ld %ld %d", scen, k, n);
    for (int i = 0; i < n; ++i) std::printf(" %.17g %.17g %.17g %.17g", full.x[i], full.y[i], full.vx[i], full.vy[i]);
    for (int i = 0; i < n; ++i) std::printf(" %.17g %.17g %.17g %.17g", h2.x[i], h2.y[i], h2.vx[i], h2.vy[i]);
    std::printf("\n");
#else
    CartIn in{}; double fmag;
    is >> in.dt >> in.mc >> in.mp >> in.l >> in.g >> fmag >> in.x >> in.v >> in.th >> in.w;
    std::printf("%ld %ld", scen, k);
    for (double sign : {1.0, -1.0}) {
      in.u = sign * fmag;
      CartOut full{}, h1{}, h2{};
      ref_step_cartpole(in, &full);
      CartIn half = in; half.dt = in.dt / 2;
      ref_step_cartpole(half, &h1);
      CartIn second = half; second.x = h1.x; second.v = h1.v; second.th = h1.th; second.w = h1.w;
      ref_step_cartpole(second, &h2);
      std::printf(" %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g", full.x, full.v, full.th, full.w, h2.x, h2.v, h2.th, h2.w);
    }
    std::printf("\n");
#endif
  }
  return 0;
}
