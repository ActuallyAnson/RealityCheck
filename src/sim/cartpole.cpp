#include "sim/cartpole.hpp"

#include <algorithm>
#include <cmath>

#include "util/rng.hpp"

namespace rc {

Json cartpole_to_json(const Scenario& s) {
  const CartPoleDef& c = s.cp;
  Json j = Json::object();
  j["domain"] = "cartpole";
  j["seed"] = static_cast<double>(s.seed);
  j["dt"] = s.dt;
  j["steps"] = s.steps;
  Json plant = Json::object();
  plant["cart_mass"] = c.mc; plant["pole_mass"] = c.mp; plant["pole_length"] = c.l; plant["gravity"] = c.g;
  j["plant"] = plant;
  Json init = Json::object();
  init["x"] = c.x0; init["v"] = c.v0; init["theta"] = c.th0; init["omega"] = c.w0;
  j["initial"] = init;
  j["force_limit"] = c.fmax;
  j["sensor_noise"] = c.noise;
  Json pushes = Json::array();
  for (const auto& p : c.pushes) {
    Json o = Json::object();
    o["t"] = p.t; o["len"] = p.len; o["force"] = p.force;
    pushes.push(o);
  }
  j["pushes"] = pushes;
  return j;
}

Scenario cartpole_from_json(const Json& j) {
  Scenario s;
  s.domain = "cartpole";
  s.seed = clamp_u64(j.num_or("seed", 1));
  s.dt = j.num_or("dt", 0.01);
  s.steps = clamp_int(j.num_or("steps", 1000));
  CartPoleDef& c = s.cp;
  if (j.has("plant")) {
    const Json& p = j.at("plant");
    c.mc = p.num_or("cart_mass", c.mc); c.mp = p.num_or("pole_mass", c.mp);
    c.l = p.num_or("pole_length", c.l); c.g = p.num_or("gravity", c.g);
  }
  if (j.has("initial")) {
    const Json& p = j.at("initial");
    c.x0 = p.num_or("x", 0); c.v0 = p.num_or("v", 0); c.th0 = p.num_or("theta", 0); c.w0 = p.num_or("omega", 0);
  }
  c.fmax = j.num_or("force_limit", c.fmax);
  c.noise = j.num_or("sensor_noise", 0);
  if (j.has("pushes")) {
    for (const auto& o : j.at("pushes").arr()) {
      PushDef p;
      p.t = o.num_or("t", 0); p.len = o.num_or("len", 0.1); p.force = o.num_or("force", 0);
      c.pushes.push_back(p);
    }
  }
  return s;
}

std::string cartpole_validate(const Scenario& s) {
  using L = CartPoleLimits;
  const CartPoleDef& c = s.cp;
  auto in = [](double v, double lo, double hi) { return std::isfinite(v) && v >= lo && v <= hi; };
  if (!in(s.dt, L::dt_min, L::dt_max)) return "dt outside [0.002, 0.02]";
  if (s.steps < 1 || s.steps > L::steps_max) return "steps outside [1, 4000]";
  if (!in(c.mc, L::mc_min, L::mc_max)) return "cart_mass outside [0.5, 2]";
  if (!in(c.mp, L::mp_min, L::mp_max)) return "pole_mass outside [0.05, 0.3]";
  if (!in(c.l, L::l_min, L::l_max)) return "pole_length outside [0.4, 1]";
  if (!in(c.g, L::g_min, L::g_max)) return "gravity outside [5, 12]";
  if (!in(c.x0, -L::x0_max, L::x0_max)) return "initial x outside [-0.5, 0.5]";
  if (!in(c.v0, -L::v0_max, L::v0_max)) return "initial v outside [-0.5, 0.5]";
  if (!in(c.th0, -L::th0_max, L::th0_max)) return "initial theta outside [-0.1, 0.1]";
  if (!in(c.w0, -L::w0_max, L::w0_max)) return "initial omega outside [-0.3, 0.3]";
  if (!in(c.fmax, L::fmax_min, L::fmax_max)) return "force_limit outside [40, 80]";
  if (!in(c.noise, 0, L::noise_max)) return "sensor_noise outside [0, 0.02]";
  if (static_cast<int>(c.pushes.size()) > L::pushes_max) return "at most 3 pushes";
  for (const auto& p : c.pushes) {
    if (!in(p.t, 0, s.dt * s.steps)) return "push time outside the run";
    if (!in(p.len, s.dt, L::push_len_max)) return "push len outside [dt, 0.2]";
    if (!in(p.force, -L::push_force_max, L::push_force_max)) return "push force outside [-4, 4]";
  }
  // overlapping pushes add up: the total force acting at any instant must stay within the same limit
  for (const auto& p : c.pushes) {
    double total = 0;
    for (const auto& q : c.pushes) if (p.t >= q.t && p.t < q.t + q.len) total += q.force;
    if (std::fabs(total) > L::push_force_max + 1e-9) return "overlapping pushes add up to more than 4 N";
  }
  return "";
}

namespace {

struct State { double x, v, th, w; };

void deriv(const State& s, double u, const CartPoleDef& c, double d[4]) {
  double st = std::sin(s.th), ct = std::cos(s.th), den = c.mc + c.mp * st * st;
  d[0] = s.v;
  d[1] = (u + c.mp * st * (c.l * s.w * s.w - c.g * ct)) / den;
  d[2] = s.w;
  d[3] = ((c.mc + c.mp) * c.g * st - ct * (u + c.mp * c.l * s.w * s.w * st)) / (c.l * den);
}

void rk4(State& s, double u, const CartPoleDef& c, double h) {
  double k1[4], k2[4], k3[4], k4[4];
  deriv(s, u, c, k1);
  deriv({s.x + h / 2 * k1[0], s.v + h / 2 * k1[1], s.th + h / 2 * k1[2], s.w + h / 2 * k1[3]}, u, c, k2);
  deriv({s.x + h / 2 * k2[0], s.v + h / 2 * k2[1], s.th + h / 2 * k2[2], s.w + h / 2 * k2[3]}, u, c, k3);
  deriv({s.x + h * k3[0], s.v + h * k3[1], s.th + h * k3[2], s.w + h * k3[3]}, u, c, k4);
  s.x += h / 6 * (k1[0] + 2 * k2[0] + 2 * k3[0] + k4[0]);
  s.v += h / 6 * (k1[1] + 2 * k2[1] + 2 * k3[1] + k4[1]);
  s.th += h / 6 * (k1[2] + 2 * k2[2] + 2 * k3[2] + k4[2]);
  s.w += h / 6 * (k1[3] + 2 * k2[3] + 2 * k3[3] + k4[3]);
}

// State feedback u = Kx*x + Kv*v + Kth*theta + Kw*omega. The gains place the four closed-loop poles of the
// linearised plant at fixed locations (derivation: characteristic polynomial of the linearised cart-pole is
// l*mc*s^4 + (Kw - l*Kv) s^3 + (Kth - (mc+mp) g - l*Kx) s^2 + g*Kv s + g*Kx), so the controller adapts to the plant.
struct Gains { double kx, kv, kth, kw; };
Gains design_gains(const CartPoleDef& c) {
  const double poles[4] = {2.5, 3.0, 3.5, 4.0};  // closed-loop poles at -poles[i]
  double p3 = 0, p2 = 0, p1 = 0, p0 = 1;
  for (int i = 0; i < 4; ++i) p3 += poles[i];
  for (int i = 0; i < 4; ++i) for (int j = i + 1; j < 4; ++j) p2 += poles[i] * poles[j];
  for (int i = 0; i < 4; ++i) for (int j = i + 1; j < 4; ++j) for (int k = j + 1; k < 4; ++k) p1 += poles[i] * poles[j] * poles[k];
  for (int i = 0; i < 4; ++i) p0 *= poles[i];
  const double lm = c.l * c.mc;
  Gains k;
  k.kx = lm * p0 / c.g;
  k.kv = lm * p1 / c.g;
  k.kw = lm * p3 + c.l * k.kv;
  k.kth = lm * p2 + (c.mc + c.mp) * c.g + c.l * k.kx;
  return k;
}

}  // namespace

double cartpole_energy(const CartPoleDef& c, const BodyState& q) {
  // tip velocity = cart velocity + l*omega*(cos th, -sin th) for th measured from upright
  double st = std::sin(q.y), ct = std::cos(q.y);
  double tvx = q.vx + c.l * q.vy * ct, tvy = -c.l * q.vy * st;
  return 0.5 * c.mc * q.vx * q.vx + 0.5 * c.mp * (tvx * tvx + tvy * tvy) + c.mp * c.g * c.l * ct;
}

Trace simulate_cartpole(const Scenario& sc, BugMask bugs) {
  const CartPoleDef& c = sc.cp;
  auto has = [&](int k) { return (bugs & (1u << k)) != 0; };
  Trace t;
  State s = {c.x0, c.v0, c.th0, c.w0};
  Rng rng(sc.seed);
  double work = 0, u_applied = 0;
  auto record = [&](int sub) {
    StepRecord r;
    r.body.push_back({s.x, s.th, s.v, s.w});
    r.energy = cartpole_energy(c, r.body[0]);
    r.work = work;
    r.fmag = std::fabs(u_applied);
    r.substeps = sub;
    t.rec.push_back(std::move(r));
  };
  record(1);
  const int nsub = std::max(1, static_cast<int>(std::ceil(sc.dt / 0.005 - 1e-9)));
  const double h = sc.dt / nsub;
  const double ratio = c.mp / c.mc;
  const Gains K = design_gains(c);
  for (int k = 1; k <= sc.steps; ++k) {
    double time = (k - 1) * sc.dt;
    // measurements: angle with seeded noise; the rate is exact
    double th_meas = s.th + (c.noise > 0 ? rng.range(-c.noise, c.noise) : 0.0);
    double w_meas = s.w;
    // seeded: with noisy sensing at a coarse step, the angular-rate term is dropped from the feedback law
    bool drop_rate = has(kCartDroppedRate) && c.noise > 0.008 && sc.dt > 0.012;
    double kx = K.kx;
    // seeded: heavy, long poles flip the sign of the cart-position term
    if (has(kCartPositionSign) && ratio > 0.3 && c.l > 0.85) kx = -K.kx;
    double u = kx * s.x + K.kv * s.v + K.kth * th_meas + (drop_rate ? 0.0 : K.kw * w_meas);
    double lim = c.fmax;
    // seeded: under strong pushes the clamp is applied at 1.5x the declared limit
    bool pushed = false;
    for (const auto& p : c.pushes) pushed |= std::fabs(p.force) > 2.5;
    if (has(kCartLooseClamp) && pushed) lim = 1.5 * c.fmax;
    u = std::max(-lim, std::min(lim, u));
    u_applied = u;
    for (int i = 0; i < nsub; ++i) {
      double tt = time + i * h, ext = 0;
      for (const auto& p : c.pushes) if (tt >= p.t && tt < p.t + p.len) ext += p.force;
      double v0 = s.v;
      rk4(s, u + ext, c, h);
      work += (u + ext) * 0.5 * (v0 + s.v) * h;
    }
    record(nsub);
  }
  return t;
}

Scenario random_cartpole_scenario(uint64_t seed) {
  using L = CartPoleLimits;
  Rng rng(seed);
  for (;;) {
    Scenario s;
    s.domain = "cartpole";
    s.seed = rng.next() % 1000000;
    s.dt = rng.range(L::dt_min, L::dt_max);
    s.steps = rng.irange(200, L::steps_max);
    CartPoleDef& c = s.cp;
    c.mc = rng.range(L::mc_min, L::mc_max);
    c.mp = rng.range(L::mp_min, L::mp_max);
    c.l = rng.range(L::l_min, L::l_max);
    c.g = rng.uni() < 0.7 ? 9.81 : rng.range(L::g_min, L::g_max);
    c.x0 = rng.range(-L::x0_max, L::x0_max);
    c.v0 = rng.range(-L::v0_max, L::v0_max);
    c.th0 = rng.range(-L::th0_max, L::th0_max);
    c.w0 = rng.range(-L::w0_max, L::w0_max);
    c.fmax = rng.range(L::fmax_min, L::fmax_max);
    c.noise = rng.uni() < 0.5 ? 0.0 : rng.range(0, L::noise_max);
    int np = rng.irange(0, L::pushes_max);
    for (int i = 0; i < np; ++i) {
      PushDef p;
      p.t = rng.range(0, s.dt * s.steps);
      p.len = rng.range(s.dt, L::push_len_max);
      p.force = rng.range(-L::push_force_max, L::push_force_max);
      c.pushes.push_back(p);
    }
    if (cartpole_validate(s).empty()) return s;
  }
}

}  // namespace rc
