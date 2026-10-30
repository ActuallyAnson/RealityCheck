#include "sim/traffic.hpp"

#include <algorithm>
#include <cmath>

#include "util/rng.hpp"

namespace rc {

namespace {
const char* kApproach[4] = {"N", "E", "S", "W"};
const char* kKind[3] = {"emergency", "ped", "detector_fail"};
int approach_index(const std::string& s) {
  for (int i = 0; i < 4; ++i) if (s == kApproach[i]) return i;
  return -1;
}
int kind_index(const std::string& s) {
  for (int i = 0; i < 3; ++i) if (s == kKind[i]) return i;
  return -1;
}
}  // namespace

Json traffic_to_json(const Scenario& s) {
  const TrafficDef& t = s.tr;
  Json j = Json::object();
  j["domain"] = "traffic";
  j["seed"] = static_cast<double>(s.seed);
  j["steps"] = s.steps;
  Json arr = Json::object();
  for (int i = 0; i < 4; ++i) arr[kApproach[i]] = t.arrival[i];
  j["arrival_per_second"] = arr;
  Json g = Json::object();
  g["ns"] = t.green_ns; g["ew"] = t.green_ew;
  j["green_seconds"] = g;
  j["yellow_seconds"] = t.yellow;
  j["all_red_seconds"] = t.all_red;
  j["max_green_seconds"] = t.max_green;
  j["actuated"] = t.actuated;
  Json ev = Json::array();
  for (const auto& e : t.events) {
    Json o = Json::object();
    o["t"] = e.t; o["kind"] = kKind[std::max(0, std::min(2, e.kind))]; o["approach"] = kApproach[std::max(0, std::min(3, e.approach))];
    o["duration"] = e.duration;
    ev.push(o);
  }
  j["events"] = ev;
  return j;
}

Scenario traffic_from_json(const Json& j) {
  Scenario s;
  s.domain = "traffic";
  s.seed = clamp_u64(j.num_or("seed", 1));
  s.steps = clamp_int(j.num_or("steps", 600));
  TrafficDef& t = s.tr;
  if (j.has("arrival_per_second")) {
    const Json& a = j.at("arrival_per_second");
    for (int i = 0; i < 4; ++i) t.arrival[i] = a.num_or(kApproach[i], t.arrival[i]);
  }
  if (j.has("green_seconds")) {
    t.green_ns = clamp_int(j.at("green_seconds").num_or("ns", t.green_ns));
    t.green_ew = clamp_int(j.at("green_seconds").num_or("ew", t.green_ew));
  }
  t.yellow = clamp_int(j.num_or("yellow_seconds", t.yellow));
  t.all_red = clamp_int(j.num_or("all_red_seconds", t.all_red));
  t.max_green = clamp_int(j.num_or("max_green_seconds", t.max_green));
  t.actuated = j.bool_or("actuated", true);
  if (j.has("events")) {
    for (const auto& o : j.at("events").arr()) {
      TrafficEvent e;
      e.t = clamp_int(o.num_or("t", 0));
      e.kind = kind_index(o.str_or("kind", "emergency"));
      e.approach = approach_index(o.str_or("approach", "N"));
      e.duration = clamp_int(o.num_or("duration", 10));
      t.events.push_back(e);  // an unknown kind/approach becomes -1 and is rejected by validate()
    }
  }
  return s;
}

std::string traffic_validate(const Scenario& s) {
  using L = TrafficLimits;
  const TrafficDef& t = s.tr;
  if (s.steps < L::steps_min || s.steps > L::steps_max) return "steps outside [60, 3000]";
  for (int i = 0; i < 4; ++i)
    if (!std::isfinite(t.arrival[i]) || t.arrival[i] < 0 || t.arrival[i] > L::arrival_max) return "arrival rate outside [0, 0.4] per second";
  if (t.green_ns < L::green_min || t.green_ns > L::green_max || t.green_ew < L::green_min || t.green_ew > L::green_max) return "green_seconds outside [8, 45]";
  if (t.yellow < L::yellow_min || t.yellow > L::yellow_max) return "yellow_seconds outside [3, 6]";
  if (t.all_red < L::all_red_min || t.all_red > L::all_red_max) return "all_red_seconds outside [1, 4]";
  if (t.max_green < L::max_green_min || t.max_green > L::max_green_max) return "max_green_seconds outside [45, 90]";
  if (t.max_green < std::max(t.green_ns, t.green_ew)) return "max_green_seconds below a green time";
  if (static_cast<int>(t.events.size()) > L::events_max) return "at most 6 events";
  int emer = 0, ped = 0;
  for (const auto& e : t.events) {
    if (e.kind < 0 || e.kind > 2) return "unknown event kind";
    if (e.approach < 0 || e.approach > 3) return "unknown approach";
    if (e.t < 0 || e.t >= s.steps) return "event time outside the run";
    if (e.kind == 0) {
      ++emer;
      if (e.duration < 1 || e.duration > L::emergency_len_max) return "emergency duration outside [1, 30]";
    } else if (e.kind == 1) {
      ++ped;
    } else if (e.duration < 1 || e.duration > L::failure_len_max) {
      return "detector_fail duration outside [1, 220]";
    }
  }
  if (emer > L::emergencies_max) return "at most 2 emergency events";
  if (ped > L::peds_max) return "at most 2 pedestrian requests";
  return "";
}

namespace {

enum Phase { NS_G = 0, NS_Y = 1, AR1 = 2, EW_G = 3, EW_Y = 4, AR2 = 5 };

struct Sim {
  const Scenario& sc;
  BugMask bugs;
  Rng rng;
  int phase = NS_G, timer = 0, yellow_len = 0, ar_len = 0;
  bool ped_pending = false, walking = false;
  int queue[4] = {0, 0, 0, 0}, wait[4] = {0, 0, 0, 0};
  double credit[4] = {0, 0, 0, 0};

  Sim(const Scenario& s, BugMask b) : sc(s), bugs(b), rng(s.seed) {
    yellow_len = s.tr.yellow;
    ar_len = s.tr.all_red;
  }
  bool has(int k) const { return (bugs & (1u << k)) != 0; }

  static int dir_of_phase(int p) { return (p == NS_G || p == NS_Y || p == AR1) ? 0 : 1; }  // direction being served or just ended
  bool green(int p) const { return p == NS_G || p == EW_G; }
  bool yellow(int p) const { return p == NS_Y || p == EW_Y; }

  bool active(int kind, int approach, int t) const {
    for (const auto& e : sc.tr.events)
      if (e.kind == kind && (approach < 0 || e.approach == approach) && t >= e.t && t < e.t + e.duration) return true;
    return false;
  }
  int emergency_dir(int t) const {  // direction (0 NS, 1 EW) of an active emergency vehicle, or -1
    for (const auto& e : sc.tr.events)
      if (e.kind == 0 && t >= e.t && t < e.t + e.duration) return e.approach % 2 == 0 ? 0 : 1;
    return -1;
  }
  int visible_queue(int dir, int t) const {  // vehicles the detectors can see on a direction
    int q = 0;
    for (int a = dir; a < 4; a += 2) if (!active(2, a, t)) q += queue[a];
    return q;
  }
  int max_wait(int dir) const {
    int w = 0;
    for (int a = dir; a < 4; a += 2) w = std::max(w, wait[a]);
    return w;
  }
  int signal(int a) const {
    int dir = a % 2;
    int g = dir == 0 ? NS_G : EW_G, y = dir == 0 ? NS_Y : EW_Y;
    return phase == g ? 2 : (phase == y ? 1 : 0);
  }

  void enter(int p, int t) {
    phase = p;
    timer = 0;
    if (p == NS_Y || p == EW_Y) yellow_len = sc.tr.yellow;
    if (p == AR1 || p == AR2) {
      walking = ped_pending;
      ar_len = sc.tr.all_red + (ped_pending ? TrafficLimits::ped_walk : 0);
      ped_pending = false;
    }
    (void)t;
  }

  void control(int t) {
    const TrafficDef& d = sc.tr;
    ++timer;
    for (const auto& e : d.events) if (e.kind == 1 && e.t == t) ped_pending = true;
    const int emer = emergency_dir(t);
    if (green(phase)) {
      int dir = dir_of_phase(phase), other = 1 - dir;
      int min_g = dir == 0 ? d.green_ns : d.green_ew;
      if (emer == dir) return;  // hold green for the emergency vehicle
      bool want_switch = false, gap_out = false;
      if (emer == other) want_switch = true;  // pre-empt: serve the emergency direction now
      else if (!d.actuated) want_switch = timer >= min_g;
      else {
        // seeded: while a detector on this direction has failed, neither max green nor the wait recall is enforced
        bool failed_here = has(kTrafficMaxGreenIgnored) && (active(2, dir, t) || active(2, dir + 2, t));
        bool recall = max_wait(other) >= 100 && !failed_here;
        bool own_empty = visible_queue(dir, t) == 0, other_demand = visible_queue(other, t) > 0;
        bool at_max = timer >= d.max_green && !failed_here;
        if (timer >= min_g && ((own_empty && (other_demand || recall)) || recall)) { want_switch = true; gap_out = own_empty && !recall; }
        if (at_max) want_switch = true;
      }
      if (want_switch) {
        enter(dir == 0 ? NS_Y : EW_Y, t);
        // seeded: a long green cut short by the actuated gap-out gets a shortened yellow
        if (has(kTrafficShortYellow) && gap_out && timer == 0 && last_green_len >= 60 && d.max_green >= 80) yellow_len = std::max(1, d.yellow - 2);
      }
      return;
    }
    if (yellow(phase)) {
      int dir = dir_of_phase(phase);
      // seeded: a pre-emption request arriving during the other direction's yellow jumps straight to green
      if (has(kTrafficPreemptSkipsClear) && emer == 1 - dir && d.yellow >= 5) { enter(dir == 0 ? EW_G : NS_G, t); last_green_len = 0; return; }
      if (timer >= yellow_len) enter(dir == 0 ? AR1 : AR2, t);
      return;
    }
    if (timer >= ar_len) {
      int next_dir = phase == AR1 ? 1 : 0;
      enter(next_dir == 0 ? NS_G : EW_G, t);
      last_green_len = 0;
    }
  }
  int last_green_len = 0;
};

}  // namespace

Trace simulate_traffic(const Scenario& sc, BugMask bugs) {
  Sim s(sc, bugs);
  Trace tr;
  auto record = [&](int t) {
    StepRecord r;
    for (int a = 0; a < 4; ++a) {
      BodyState q;
      q.x = s.signal(a);
      q.y = s.queue[a];
      q.vx = s.wait[a];
      q.vy = (s.active(0, a, t) || (s.walking && (s.phase == AR1 || s.phase == AR2))) ? 1 : 0;
      r.body.push_back(q);
    }
    r.fmag = s.phase;
    tr.rec.push_back(std::move(r));
  };
  record(0);
  for (int t = 1; t <= sc.steps; ++t) {
    for (int a = 0; a < 4; ++a)
      if (s.rng.uni() < sc.tr.arrival[a]) s.queue[a] = std::min(200, s.queue[a] + 1);
    s.control(t);
    if (s.green(s.phase)) ++s.last_green_len;
    for (int a = 0; a < 4; ++a) {
      if (s.signal(a) == 2 && s.queue[a] > 0) {
        s.credit[a] += 0.5;  // discharge: one vehicle every two seconds of green
        while (s.credit[a] >= 1 && s.queue[a] > 0) { --s.queue[a]; s.credit[a] -= 1; }
      } else if (s.signal(a) != 2) {
        s.credit[a] = 0;
      }
      if (s.signal(a) == 0 && s.queue[a] > 0) ++s.wait[a];
    }
    record(t);
  }
  return tr;
}

Scenario random_traffic_scenario(uint64_t seed) {
  using L = TrafficLimits;
  Rng rng(seed);
  for (;;) {
    Scenario s;
    s.domain = "traffic";
    s.seed = rng.next() % 1000000;
    s.steps = rng.irange(L::steps_min, L::steps_max);
    TrafficDef& t = s.tr;
    for (int i = 0; i < 4; ++i) t.arrival[i] = rng.uni() < 0.2 ? 0.0 : rng.range(0, L::arrival_max);
    t.green_ns = rng.irange(L::green_min, L::green_max);
    t.green_ew = rng.irange(L::green_min, L::green_max);
    t.yellow = rng.irange(L::yellow_min, L::yellow_max);
    t.all_red = rng.irange(L::all_red_min, L::all_red_max);
    t.max_green = rng.irange(std::max(L::max_green_min, std::max(t.green_ns, t.green_ew)), L::max_green_max);
    t.actuated = rng.uni() < 0.7;
    int ne = rng.irange(0, L::events_max);
    for (int i = 0; i < ne; ++i) {
      TrafficEvent e;
      e.kind = rng.irange(0, 2);
      e.approach = rng.irange(0, 3);
      e.t = rng.irange(0, s.steps - 1);
      e.duration = e.kind == 0 ? rng.irange(1, L::emergency_len_max) : e.kind == 2 ? rng.irange(1, L::failure_len_max) : 1;
      t.events.push_back(e);
    }
    if (traffic_validate(s).empty()) return s;
  }
}

}  // namespace rc
