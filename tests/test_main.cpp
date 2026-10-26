// Self-contained test runner (no framework dependency).
#include <cmath>
#include <functional>
#include <iostream>

#include "baseline/random_baseline.hpp"
#include "eval/ground_truth.hpp"
#include "rules/checker.hpp"
#include "shrink/shrink.hpp"
#include "sim/world.hpp"
#include "tools/render.hpp"
#include "tools/tools.hpp"

using namespace rc;

static int g_fail = 0, g_checks = 0;
#define CHECK(cond)                                                                   \
  do {                                                                                \
    ++g_checks;                                                                       \
    if (!(cond)) { ++g_fail; std::cerr << __FILE__ << ":" << __LINE__ << " FAILED: " #cond "\n"; } \
  } while (0)

static bool has_rule(const std::vector<Violation>& v, const std::string& r) {
  for (const auto& x : v) if (x.rule == r) return true;
  return false;
}

static Scenario base() {
  Scenario s;
  s.dt = 1.0 / 120;
  s.steps = 600;
  return s;
}
static BodyDef ball(double x, double y, double vx, double vy, double r = 0.5, double m = 1, double e = 0.8) {
  BodyDef b; b.x = x; b.y = y; b.vx = vx; b.vy = vy; b.r = r; b.m = m; b.e = e;
  return b;
}

// One hand-built scenario per seeded bug that triggers it. These are regression tests for the bug system;
// they are not used by any tester.
static Scenario trigger(int bug) {
  Scenario s = base();
  switch (bug) {
    case kTunnelFastThin:
      s.dt = 0.01; s.steps = 100; s.gravity = 0;
      s.walls.push_back({10, 2, 0.15, 8});
      s.bodies.push_back(ball(3, 6, 50, 0));
      break;
    case kExplicitOrder:
      s.dt = 1.0 / 30; s.steps = 4000;
      s.bodies.push_back(ball(10, 8, 0, 0, 0.5, 10, 1.0));
      break;
    case kSmallDtBlowup:
      s.dt = 1e-4; s.steps = 4000;
      s.bodies.push_back(ball(10, 3, 0, -30, 0.5, 1, 0.5));
      break;
    case kCornerNormal:
      s.dt = 0.005; s.steps = 400; s.gravity = 0;
      s.walls.push_back({10, 5, 2, 2});
      s.bodies.push_back(ball(8.2, 3.2, 6, 6, 0.6));
      break;
    case kRestitutionGain:
      s.steps = 1500;
      s.bodies.push_back(ball(10, 6, 0, 0, 0.5, 1, 0.95));
      break;
    case kRestingKick:
      s.steps = 1500;
      for (int i = 0; i < 3; ++i) s.bodies.push_back(ball(10, 0.6 + 1.05 * i, 0, 0, 0.5, 2, 0.2));
      break;
    case kMassSwap:
      s.gravity = 0; s.steps = 300;
      s.bodies.push_back(ball(5, 6, 10, 0, 0.4, 1, 0.9));
      s.bodies.push_back(ball(8, 6, 0, 0, 1.0, 20, 0.9));
      break;
    case kClampSkipNear:
      s.gravity = 0; s.steps = 300;
      s.bodies.push_back(ball(2, 6, 15, 0, 0.5, 1, 0.8));
      s.ctrl.enabled = true; s.ctrl.body = 0; s.ctrl.kp = 8; s.ctrl.fmax = 20; s.ctrl.vcmd = 6;
      s.ctrl.waypoints.push_back({6, 6});
      break;
    case kZeroDirNaN:
      s.gravity = 0; s.steps = 50;
      s.bodies.push_back(ball(5, 5, 0, 0));
      s.ctrl.enabled = true; s.ctrl.body = 0; s.ctrl.waypoints.push_back({5, 5});
      break;
    case kBigRadiusBound:
      s.gravity = 0; s.steps = 400;
      s.bodies.push_back(ball(15, 6, 10, 0, 1.0));
      break;
  }
  return s;
}

static void test_json() {
  Json j = Json::parse(R"({"a":[1,2.5,"x\n",true,null],"b":{"c":-3e2}})");
  CHECK(j.at("a").arr().size() == 5);
  CHECK(j.at("b").at("c").num() == -300);
  CHECK(Json::parse(j.dump()).dump() == j.dump());
  bool threw = false;
  try { Json::parse("{\"a\":"); } catch (const std::exception&) { threw = true; }
  CHECK(threw);
}

static void test_scenario() {
  Scenario s = base();
  s.bodies.push_back(ball(5, 5, 1, 1));
  CHECK(validate(s).empty());
  Scenario rt = scenario_from_json(Json::parse(to_json(s).dump()));
  CHECK(rt == s);
  Scenario bad = s; bad.dt = 1.0;       CHECK(!validate(bad).empty());
  bad = s; bad.bodies[0].x = 0.1;       CHECK(!validate(bad).empty());  // starts outside the arena
  bad = s; bad.bodies.push_back(ball(5.2, 5, 0, 0)); CHECK(!validate(bad).empty());  // overlap
  bad = s; bad.walls.push_back({1, 1, 0.05, 3});     CHECK(!validate(bad).empty());  // too thin
  bad = s; bad.bodies[0].e = 1.2;       CHECK(!validate(bad).empty());
}

static void test_determinism() {
  Scenario s = trigger(kClampSkipNear);
  s.ctrl.noise = 0.2; s.seed = 7;
  Trace a = simulate(s, 0), b = simulate(s, 0);
  CHECK(a.hash() == b.hash());
  s.seed = 8;
  CHECK(simulate(s, 0).hash() != a.hash());  // the seed matters when the controller is noisy
  for (int i = 0; i < 20; ++i) {
    Scenario r = random_scenario(50 + i);
    CHECK(simulate(r, 0).hash() == simulate(r, 0).hash());
  }
}

static void test_clean_sim_has_no_violations() {
  int bad = 0;
  for (int i = 0; i < 400; ++i) {
    Scenario s = random_scenario(7000 + i);
    if (!check_rules(s, simulate(s, 0)).empty()) ++bad;
  }
  CHECK(bad == 0);
}

static Trace flat_trace(const Scenario& sc, int steps) {
  Trace t;
  for (int k = 0; k <= steps; ++k) {
    StepRecord r;
    for (const auto& b : sc.bodies) r.body.push_back({b.x, b.y, b.vx, b.vy});
    r.energy = 100;
    t.rec.push_back(r);
  }
  return t;
}

static void test_checker() {
  Scenario sc = base();
  sc.bodies.push_back(ball(5, 5, 0, 0));
  sc.walls.push_back({8, 2, 0.2, 6});
  Trace ok = flat_trace(sc, 3);
  CHECK(check_rules(sc, ok).empty());

  Trace t = ok; t.rec[2].body[0].x = NAN;
  auto v = check_rules(sc, t);
  CHECK(has_rule(v, "no_nan"));
  CHECK(v.front().step == 2);

  t = ok; t.rec[1].body[0].x = 0.3;  // radius 0.5 -> 0.2 outside
  CHECK(has_rule(check_rules(sc, t), "in_arena"));
  t = ok; t.rec[1].body[0].x = 0.45; // 0.05 outside: within the 0.15 tolerance
  CHECK(!has_rule(check_rules(sc, t), "in_arena"));

  t = ok; t.rec[1].body[0].x = 7.6;  // wall at 8, radius 0.5 -> 0.1 overlap... within tolerance
  CHECK(!has_rule(check_rules(sc, t), "no_wall_penetration"));
  t = ok; t.rec[1].body[0].x = 8.2; t.rec[1].body[0].y = 5; // centre inside box
  v = check_rules(sc, t);
  CHECK(has_rule(v, "no_wall_penetration"));
  CHECK(has_rule(v, "no_tunneling"));

  t = ok; t.rec[1].body[0].x = 7.0; t.rec[2].body[0].x = 9.5; t.rec[3].body[0].x = 9.5;  // jumped across the wall
  v = check_rules(sc, t);
  CHECK(has_rule(v, "no_tunneling"));
  for (const auto& x : v) if (x.rule == "no_tunneling") CHECK(x.step == 2);

  t = ok; t.rec[2].energy = 1000;
  CHECK(has_rule(check_rules(sc, t), "energy_bound"));
  t = ok; t.rec[2].energy = 1000; t.rec[2].work = 950;  // controller work explains the gain
  CHECK(!has_rule(check_rules(sc, t), "energy_bound"));

  Scenario cs = sc; cs.ctrl.enabled = true; cs.ctrl.fmax = 50; cs.ctrl.waypoints.push_back({1, 1});
  t = ok; t.rec[1].fmag = 50.0;
  CHECK(!has_rule(check_rules(cs, t), "controller_force_limit"));
  t.rec[1].fmag = 51.0;
  CHECK(has_rule(check_rules(cs, t), "controller_force_limit"));
}

static void test_each_bug_is_detectable_and_isolated() {
  for (int b = 0; b < kNumBugs; ++b) {
    Scenario s = trigger(b);
    CHECK(validate(s).empty());
    CHECK(check_rules(s, simulate(s, 0)).empty());          // no bug: clean
    if (check_rules(s, simulate(s, bug_bit(b))).empty()) std::cerr << "trigger for " << bug_id(b) << " does not fire\n";
    CHECK(!check_rules(s, simulate(s, bug_bit(b))).empty());  // bug on: caught by the checker
    for (int other = 0; other < kNumBugs; ++other) {
      if (other == b) continue;  // other bugs must not fire on this trigger (isolation of the flags)
      if (check_rules(s, simulate(s, bug_bit(other))).empty() == false) {
        // allowed only when the other bug shares the trigger; report so mistakes are visible
        std::cerr << "note: trigger for " << bug_id(b) << " also fires " << bug_id(other) << "\n";
      }
    }
    if (g_fail) std::cerr << "  (while testing " << bug_id(b) << ")\n";
  }
}

static void test_shrinker() {
  // 1) every seeded-bug trigger: shrinking preserves the failure and never grows the scenario
  for (int b = 0; b < kNumBugs; ++b) {
    Scenario s = trigger(b);
    BugMask m = bug_bit(b);
    auto v = check_rules(s, simulate(s, m));
    CHECK(!v.empty());
    if (v.empty()) continue;
    ShrinkResult r = shrink(s, m, v.front().rule);
    CHECK(r.ok);
    if (!r.ok) continue;
    CHECK(r.size_after <= r.size_before);
    CHECK(validate(r.scenario).empty());
    CHECK(has_rule(check_rules(r.scenario, simulate(r.scenario, m)), v.front().rule));
    // the shrunk scenario is a fixed point: shrinking again keeps the failure too
    ShrinkResult r2 = shrink(r.scenario, m, v.front().rule);
    CHECK(r2.ok && r2.size_after <= r.size_after);
  }
  // 2) failures found by random testing: the invariant holds across many unrelated failures
  int checked = 0;
  for (int b = 0; b < kNumBugs && checked < 40; ++b) {
    for (int i = 0; i < 400 && checked < 40; ++i) {
      Scenario s = random_scenario(31000 + 977 * b + i);
      BugMask m = bug_bit(b);
      auto v = check_rules(s, simulate(s, m));
      if (v.empty()) continue;
      ++checked;
      ShrinkResult r = shrink(s, m, v.front().rule, 300);
      CHECK(r.ok);
      if (r.ok) {
        CHECK(has_rule(check_rules(r.scenario, simulate(r.scenario, m)), v.front().rule));
        CHECK(r.size_after <= r.size_before);
      }
    }
  }
  CHECK(checked >= 20);
  // 3) a scenario that does not fail cannot be "shrunk"
  Scenario clean = trigger(kMassSwap);
  CHECK(!shrink(clean, 0, "energy_bound").ok);
}

static void test_baseline_budget() {
  Budget bud; bud.max_steps = 20000; bud.max_wall_ms = 60000;
  HuntResult h = random_baseline(0, 5, bud);
  CHECK(!h.found);
  CHECK(h.steps_used == 20000);  // spends exactly the step budget on a clean simulation
  HuntResult h2 = random_baseline(bug_bit(kBigRadiusBound), 5, bud);
  CHECK(h2.found && h2.steps_used <= 20000);
  HuntResult h3 = random_baseline(bug_bit(kBigRadiusBound), 5, bud);
  CHECK(h3.scenarios_until_first == h2.scenarios_until_first);  // deterministic in the seed
}

static void test_tools() {
  ToolBudget tb; tb.max_steps = 5000;
  ToolSession ts(bug_bit(kBigRadiusBound), tb, ".");
  Json args = Json::object();
  args["scenario"] = to_json(trigger(kBigRadiusBound));
  Json r = ts.call("run_scenario", args);
  CHECK(r.at("ok").boolean());
  CHECK(r.at("violation_count").num() > 0);
  std::string id = r.at("scenario_id").str();
  Json a2 = Json::object(); a2["scenario_id"] = id;
  CHECK(ts.call("check_rules", a2).at("ok").boolean());
  Json a3 = a2; a3["step"] = 10;
  Json st = ts.call("inspect_state", a3);
  CHECK(st.at("ok").boolean() && st.at("bodies").arr().size() == 1);
  Json a4 = a2; a4["step"] = 99999;
  CHECK(!ts.call("inspect_state", a4).at("ok").boolean());
  Json rp = ts.call("replay", a2);
  CHECK(rp.at("identical_trace").boolean() && rp.at("identical_violations").boolean());
  Json sh = ts.call("shrink", a2);
  CHECK(sh.at("ok").boolean());
  CHECK(!ts.call("run_scenario", Json::object()).at("ok").boolean());
  CHECK(!ts.call("nonsense", Json::object()).at("ok").boolean());
  // budget: a long scenario is truncated to what remains, then further runs are refused
  Json big = Json::object();
  Scenario s = trigger(kBigRadiusBound); s.steps = 4000;
  big["scenario"] = to_json(s);
  ts.call("run_scenario", big);
  Json again = ts.call("run_scenario", big);
  CHECK(ts.usage().steps_used <= tb.max_steps);
  CHECK(again.at("ok").boolean() == false || again.at("truncated_to_budget").boolean());
  // the tester-facing output never names a seeded bug
  std::string all = r.dump() + st.dump() + sh.dump() + rp.dump() + simulation_description() + tool_specs().dump();
  for (int b = 0; b < kNumBugs; ++b) CHECK(all.find(bug_id(b)) == std::string::npos);
}

static void test_render() {
  Scenario s = trigger(kBigRadiusBound);
  Trace t = simulate(s, 0);
  auto png = render_strip_png(s, t, {0, 100, 200, 300}, 0);
  CHECK(png.size() > 1000);
  CHECK(png[0] == 0x89 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G');
  CHECK(base64_encode({'M', 'a', 'n'}) == "TWFu");
  CHECK(base64_encode({'M', 'a'}) == "TWE=");
  CHECK(base64_encode({'M'}) == "TQ==");
}

static void test_ground_truth_scoring() {
  Json gt = Json::parse(read_file("data/ground_truth.json"));
  CHECK(explanation_names_cause(gt, "b01", "A very fast ball skipped the thin wall: the speed threshold disables the collision."));
  CHECK(!explanation_names_cause(gt, "b01", "The ball got stuck in the corner."));
  CHECK(!explanation_names_cause(gt, "b05", "unrelated text"));
  for (int b = 0; b < kNumBugs; ++b) CHECK(gt.at("bugs").has(bug_id(b)));
}

int main() {
  struct T { const char* name; std::function<void()> fn; };
  T tests[] = {
      {"json", test_json}, {"scenario", test_scenario}, {"determinism", test_determinism},
      {"clean sim has no violations", test_clean_sim_has_no_violations}, {"checker", test_checker},
      {"each bug detectable", test_each_bug_is_detectable_and_isolated}, {"shrinker", test_shrinker},
      {"baseline budget", test_baseline_budget}, {"tools", test_tools}, {"render", test_render},
      {"ground truth scoring", test_ground_truth_scoring},
  };
  for (auto& t : tests) {
    int before = g_fail;
    t.fn();
    std::cout << (g_fail == before ? "[ ok ] " : "[FAIL] ") << t.name << "\n";
  }
  std::cout << g_checks << " checks, " << g_fail << " failed\n";
  return g_fail ? 1 : 0;
}
