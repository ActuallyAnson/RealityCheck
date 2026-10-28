// Self-contained test runner (no framework dependency).
#include <cmath>
#include <functional>
#include <iostream>

#include <fstream>

#include "agent/agent.hpp"
#include "baseline/random_baseline.hpp"
#include "eval/ground_truth.hpp"
#include "model/model.hpp"
#include "rules/checker.hpp"
#include "shrink/shrink.hpp"
#include "external/bundle.hpp"
#include "external/diverge.hpp"
#include "external/engine.hpp"
#include "sim/cartpole.hpp"
#include "sim/orbit.hpp"
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
    case kFastGravityOvercount:
      s.dt = 1.0 / 30; s.steps = 300;
      s.bodies.push_back(ball(10, 10, 0, -45, 0.5, 5, 0.9));
      break;
    case kWideWallShortReach:
      s.gravity = 0; s.steps = 300;
      s.walls.push_back({12, 2, 3, 6});
      s.bodies.push_back(ball(17, 5, -10, 0, 0.5));
      break;
    case kNoisyClampLoose:
      s.gravity = 0; s.steps = 300;
      s.bodies.push_back(ball(2, 6, 0, 0, 0.5, 1, 0.8));
      s.ctrl.enabled = true; s.ctrl.body = 0; s.ctrl.kp = 28; s.ctrl.fmax = 20; s.ctrl.vcmd = 6; s.ctrl.noise = 0.25;
      s.ctrl.waypoints.push_back({14, 6});
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


// ------------------------------------------------------------------ step 4: model client + agent (mock only)

static Json last_tool_json(const ModelRequest& req) {
  for (size_t i = req.messages.size(); i-- > 0;)
    if (req.messages[i].role == "tool") return Json::parse(req.messages[i].content);
  return Json::object();
}

static void test_wire_format() {
  ModelRequest req;
  req.model = "muse-spark-1.3";
  ChatMessage sys; sys.role = "system"; sys.content = "hi";
  ChatMessage u; u.role = "user"; u.content = "look"; u.images_b64 = {"QUJD"};
  ChatMessage a; a.role = "assistant"; a.tool_calls.push_back({"c1", "run_scenario", "{\"x\":1}"});
  ChatMessage t; t.role = "tool"; t.tool_call_id = "c1"; t.content = "{}";
  req.messages = {sys, u, a, t};
  req.tools = tool_specs();
  Json j = build_chat_request(req);
  CHECK(j.at("model").str() == "muse-spark-1.3");
  CHECK(!j.has("tool_choice"));  // the API only supports "auto"
  CHECK(j.at("tools").arr().size() == 6);
  CHECK(j.at("tools").arr()[0].at("type").str() == "function");
  CHECK(j.at("tools").arr()[0].at("function").at("name").str() == "run_scenario");
  const Json& um = j.at("messages").arr()[1];
  CHECK(um.at("content").arr()[1].at("image_url").at("url").str() == "data:image/png;base64,QUJD");
  const Json& am = j.at("messages").arr()[2];
  CHECK(am.at("content").is_null());
  CHECK(am.at("tool_calls").arr()[0].at("function").at("arguments").is_string());
  CHECK(j.at("messages").arr()[3].at("tool_call_id").str() == "c1");
  CHECK(request_key(req) == request_key(req));

  ModelResponse r = parse_chat_response(Json::parse(
      R"({"choices":[{"finish_reason":"tool_calls","message":{"content":null,"tool_calls":[{"id":"abc","type":"function",
      "function":{"name":"shrink","arguments":"{\"scenario_id\":\"s1\"}"}}]}}],
      "usage":{"prompt_tokens":1000,"completion_tokens":500,"prompt_tokens_details":{"cached_tokens":200},
      "completion_tokens_details":{"reasoning_tokens":300}}})"));
  CHECK(r.ok && r.message.tool_calls.size() == 1 && r.message.tool_calls[0].name == "shrink");
  CHECK(r.usage.prompt_tokens == 1000 && r.usage.cached_tokens == 200 && r.usage.reasoning_tokens == 300);
  CHECK(!parse_chat_response(Json::parse(R"({"error":{"message":"rate limited"}})")).ok);
  ModelResponse rt = response_from_json(Json::parse(response_to_json(r).dump()));
  CHECK(rt.ok && rt.message.tool_calls[0].id == "abc" && rt.usage.completion_tokens == 500);
  Pricing p;
  double expect = (800 * 1.25 + 200 * 0.15 + 500 * 4.25) / 1e6;
  CHECK(std::fabs(cost_usd(r.usage, p) - expect) < 1e-12);
  ApiConfig c = ApiConfig::from_json(Json::parse(read_file("configs/open_weight.json")));
  CHECK(c.base_url == "https://inference.baseten.co/v1" && c.api_key_env == "BASETEN_API_KEY");
  ApiConfig h = ApiConfig::from_json(Json::parse(read_file("configs/hosted.json")));
  CHECK(h.api_key_env == "MODEL_API_KEY" && h.model == "muse-spark-1.3");
}

static void test_live_backend_refuses_without_key() {
  ApiConfig cfg;
  cfg.api_key_env = "RC_TEST_KEY_THAT_IS_NOT_SET";
  MetaApiBackend b(cfg);
  ModelRequest req;
  ModelResponse r = b.complete(req);
  CHECK(!r.ok && r.error.find("not set") != std::string::npos);  // returns before any network access
}

static void test_budget() {
  MockBackend mock([](const ModelRequest&, int) { return MockBackend::text("ok", 10, 1000); });
  std::remove("test_cost.jsonl");
  BudgetedModel bm(mock, Pricing(), 0.0100, "test_cost.jsonl");
  ModelRequest req;
  req.max_tokens = 1000;  // the pre-check assumes the call may use all of max_tokens
  ChatMessage m; m.role = "user"; m.content = "x";
  req.messages = {m};
  int made = 0;
  bool stopped = false;
  try {
    for (int i = 0; i < 20; ++i) { bm.complete(req, "t"); ++made; }
  } catch (const BudgetExceeded&) { stopped = true; }
  CHECK(stopped);
  CHECK(bm.spent() <= 0.0100 + 1e-12);
  CHECK(mock.calls() == made);  // the refused call never reached the backend
  CHECK(made == 2);             // (10*1.25 + 1000*4.25)/1e6 = 0.00426 per call; a third call could cost up to 0.0043 more than the budget allows
  std::ifstream f("test_cost.jsonl");
  int lines = 0;
  std::string line;
  while (std::getline(f, line)) { ++lines; Json j = Json::parse(line); CHECK(j.at("cost_usd").num() > 0); }
  CHECK(lines == made);
  std::remove("test_cost.jsonl");
}

static Json spent_report(const std::string& id, int step, double x, const std::string& explanation) {
  Json cit = Json::object();
  cit["step"] = step; cit["body"] = 0; cit["field"] = "x"; cit["value"] = x; cit["claim"] = "position";
  Json cits = Json::array(); cits.push(cit);
  Json r = Json::object();
  r["scenario_id"] = id; r["rule"] = "in_arena"; r["explanation"] = explanation; r["citations"] = cits;
  return r;
}

static void test_citation_validation() {
  ToolBudget tb;
  ToolSession ts(bug_bit(kBigRadiusBound), tb, ".");
  Json a = Json::object(); a["scenario"] = to_json(trigger(kBigRadiusBound));
  std::string id0 = ts.call("run_scenario", a).at("scenario_id").str();
  Json sa = Json::object(); sa["scenario_id"] = id0;
  Json sh = ts.call("shrink", sa);
  std::string id = sh.at("scenario_id").str();
  int vstep = static_cast<int>(sh.at("violation").at("step").num());
  const ToolSession::Entry* run = ts.record(id);
  CHECK(run != nullptr);
  double x = run->tr.rec[static_cast<size_t>(vstep)].body[0].x;
  std::string good = "Body 0 crosses the right boundary by step " + std::to_string(vstep) + ", centre beyond the wall.";
  CHECK(validate_report(*run, true, spent_report(id, vstep, x, good)).ok);
  // not shrunk
  CHECK(!validate_report(*run, false, spent_report(id, vstep, x, good)).ok);
  // nonexistent step
  CHECK(!validate_report(*run, true, spent_report(id, run->tr.steps() + 1, x, good)).ok);
  // nonexistent step only in the prose
  CHECK(!validate_report(*run, true, spent_report(id, vstep, x, "Body 0 leaves the arena at step 987654 which is late.")).ok);
  // nonexistent body in prose
  CHECK(!validate_report(*run, true, spent_report(id, vstep, x, "Body 5 leaves the arena at step " + std::to_string(vstep) + ".")).ok);
  // wrong value
  CHECK(!validate_report(*run, true, spent_report(id, vstep, x + 3.0, good)).ok);
  // no citation of the violating step
  if (vstep > 0) CHECK(!validate_report(*run, true, spent_report(id, vstep - 1, run->tr.rec[static_cast<size_t>(vstep - 1)].body[0].x, "Body 0 is near the boundary before it fails.")).ok);
  // wrong rule
  Json wrong = spent_report(id, vstep, x, good); wrong["rule"] = "energy_bound";
  CHECK(!validate_report(*run, true, wrong).ok);
  // missing citations
  Json none = spent_report(id, vstep, x, good); none["citations"] = Json::array();
  CHECK(!validate_report(*run, true, none).ok);
}

struct ScriptState {
  std::string shrunk_id;
  int vstep = 0;
};

static MockBackend::Script hunt_script(ScriptState& st) {
  return [&st](const ModelRequest& req, int i) -> ModelResponse {
    switch (i) {
      case 0: {
        Json a = Json::object(); a["scenario"] = to_json(trigger(kBigRadiusBound));
        ModelResponse r = MockBackend::tool("run_scenario", a, "call0");
        r.message.content = "Hypothesis: a large body moving fast toward the right wall may leave the arena.";
        return r;
      }
      case 1: {
        Json a = Json::object(); a["scenario_id"] = last_tool_json(req).at("scenario_id").str();
        return MockBackend::tool("shrink", a, "call1");
      }
      case 2: {
        Json j = last_tool_json(req);
        st.shrunk_id = j.at("scenario_id").str();
        st.vstep = static_cast<int>(j.at("violation").at("step").num());
        Json a = Json::object(); a["scenario_id"] = st.shrunk_id; a["step"] = st.vstep;
        return MockBackend::tool("inspect_state", a, "call2");
      }
      case 3: {  // first report cites a step that does not exist: must be rejected
        Json j = last_tool_json(req);
        double x = j.at("bodies").arr()[0].at("x").num();
        return MockBackend::tool("submit_report",
                                 spent_report(st.shrunk_id, 999999, x, "Body 0 passes the right boundary at step 999999."), "call3");
      }
      case 4: {
        Json j = last_tool_json(req);
        CHECK(j.at("ok").boolean() == false);
        CHECK(j.at("problems").arr().size() >= 1);
        Json a = Json::object(); a["scenario_id"] = st.shrunk_id; a["step"] = st.vstep;
        return MockBackend::tool("inspect_state", a, "call4");
      }
      case 5: {
        double x = last_tool_json(req).at("bodies").arr()[0].at("x").num();
        return MockBackend::tool(
            "submit_report",
            spent_report(st.shrunk_id, st.vstep, x, "Body 0 ends step " + std::to_string(st.vstep) + " past the right wall."), "call5");
      }
      default:
        return MockBackend::text(R"({"description":"The red ball is half outside the right edge.","agrees":true,"disagreement":""})");
    }
  };
}

static void test_agent_end_to_end_mock() {
  ScriptState st;
  MockBackend mock(hunt_script(st));
  std::remove("test_agent_cost.jsonl");
  BudgetedModel bm(mock, Pricing(), 1.0, "test_agent_cost.jsonl");
  ToolBudget tb; tb.max_steps = 20000;
  ToolSession ts(bug_bit(kBigRadiusBound), tb, ".");
  AgentConfig cfg;
  std::vector<Json> events;
  AgentResult r = run_agent(bm, ts, cfg, [&](const Json& e) { events.push_back(e); });
  CHECK(r.stop_reason == "report_accepted");
  CHECK(r.found_failure && r.report_submitted && r.report_valid);
  CHECK(r.report_attempts == 2 && r.rejected_reports == 1);
  CHECK(r.rule == "in_arena");
  CHECK(r.vision_done && r.vision_agrees);
  CHECK(!r.frame_strip_path.empty());
  CHECK(bm.calls() == 7);  // 6 hunt turns + 1 vision call
  double per_call = (800 * 1.25 + 120 * 4.25) / 1e6;
  CHECK(std::fabs(bm.spent() - 7 * per_call) < 1e-9);
  std::ifstream f("test_agent_cost.jsonl");
  int lines = 0;
  std::string line;
  while (std::getline(f, line)) ++lines;
  CHECK(lines == 7);
  bool saw_rejection = false;
  std::string all;
  for (const auto& e : events) { all += e.dump(); if (e.str_or("type", "") == "report_rejected") saw_rejection = true; }
  CHECK(saw_rejection);
  for (int b = 0; b < kNumBugs; ++b) CHECK(all.find(bug_id(b)) == std::string::npos);  // no ground truth leaks into the transcript
  write_report_files(r, ts, ".");
  CHECK(!read_file("report.json").empty() && !read_file("trace.json").empty());
  std::remove("report.json"); std::remove("trace.json"); std::remove("test_agent_cost.jsonl");

  // The system prompt shown to the model carries no bug information.
  std::string sp = system_prompt(true) + system_prompt(false);
  for (int b = 0; b < kNumBugs; ++b) CHECK(sp.find(bug_id(b)) == std::string::npos);
}

static void test_agent_model_budget_stop() {
  ScriptState st;
  MockBackend mock(hunt_script(st));
  BudgetedModel bm(mock, Pricing(), 0.004);  // fits two scripted calls, not three
  ToolSession ts(bug_bit(kBigRadiusBound), ToolBudget(), ".");
  AgentResult r = run_agent(bm, ts, AgentConfig());
  CHECK(r.stop_reason == "model_budget");
  CHECK(!r.report_valid);
  CHECK(bm.spent() <= 0.004);
}

static void test_agent_json_protocol_mode() {
  MockBackend mock([](const ModelRequest& req, int i) -> ModelResponse {
    CHECK(req.tools.is_null() || req.tools.arr().empty());  // no native tools in this mode
    if (i == 0) {
      Json a = Json::object(); a["tool"] = "run_scenario"; Json args = Json::object();
      args["scenario"] = to_json(trigger(kBigRadiusBound)); a["args"] = args;
      return MockBackend::text("I will test a fast large body.\n```json\n" + a.dump() + "\n```");
    }
    return MockBackend::text("I am done.");
  });
  BudgetedModel bm(mock, Pricing(), 1.0);
  ToolSession ts(bug_bit(kBigRadiusBound), ToolBudget(), ".");
  AgentConfig cfg; cfg.native_tools = false;
  AgentResult r = run_agent(bm, ts, cfg);
  CHECK(ts.usage().scenarios_run == 1);
  CHECK(r.found_failure);
  CHECK(r.stop_reason == "no_tool_calls");
}

static void test_record_and_replay() {
  std::remove("test_cache.jsonl");
  AgentResult first;
  double first_cost = 0;
  {
    ScriptState st;
    MockBackend mock(hunt_script(st));
    RecordingBackend rec(mock, "test_cache.jsonl");
    BudgetedModel bm(rec, Pricing(), 1.0);
    ToolSession ts(bug_bit(kBigRadiusBound), ToolBudget(), ".");
    first = run_agent(bm, ts, AgentConfig());
    first_cost = bm.spent();
  }
  CHECK(first.report_valid);
  ReplayBackend replay("test_cache.jsonl");
  CHECK(replay.entries() == 7);
  BudgetedModel bm2(replay, Pricing(), 1.0);
  ToolSession ts2(bug_bit(kBigRadiusBound), ToolBudget(), ".");
  AgentResult second = run_agent(bm2, ts2, AgentConfig());
  CHECK(second.report_valid && second.explanation == first.explanation && second.scenario_id == first.scenario_id);
  CHECK(std::fabs(bm2.spent() - first_cost) < 1e-12);
  // a request the cache has never seen is an error, never a live call
  ModelRequest odd;
  ChatMessage m; m.role = "user"; m.content = "never recorded";
  odd.messages = {m};
  ModelResponse miss = replay.complete(odd);
  CHECK(!miss.ok && miss.error.find("cache miss") != std::string::npos);
  std::remove("test_cache.jsonl");
}


static void test_lifetime_ledger() {
  std::string path = "test_ledger.jsonl";
  std::remove(path.c_str());
  MockBackend be([](const ModelRequest&, int) { return MockBackend::text("ok", 100000, 1000); });  // ~$0.13 per call
  Pricing pr;
  {
    BudgetedModel m(be, pr, 100.0);
    m.set_lifetime_ledger(path, 0.30);
    ModelRequest rq;
    rq.max_tokens = 100;
    rq.messages.push_back({"user", "hi", {}, "", {}});
    m.complete(rq, "a");
    m.complete(rq, "b");
    CHECK(ledger_total_usd(path) > 0.25);
    bool refused = false;
    rq.max_tokens = 20000;  // worst case now ~$0.085 on top of ~$0.26 spent
    try { m.complete(rq, "c"); } catch (const BudgetExceeded&) { refused = true; }
    CHECK(refused);
  }
  {  // a second process-like instance sees the same total
    BudgetedModel m2(be, pr, 100.0);
    m2.set_lifetime_ledger(path, 0.30);
    ModelRequest rq;
    rq.max_tokens = 100;
    rq.messages.push_back({"user", "hi", {}, "", {}});
    bool refused = false;
    rq.max_tokens = 20000;
    try { m2.complete(rq, "d"); } catch (const BudgetExceeded&) { refused = true; }
    CHECK(refused);
  }
  CHECK(ledger_total_usd("does_not_exist.jsonl") == 0.0);
  std::remove(path.c_str());
}


// Frozen rigid2d fingerprints: replay caches key on the exact prompt/schema bytes and the locked
// evaluation depends on trace/scenario hashes. Adding domains must not change any of these.
static std::string pin_string() {
  std::string o;
  auto h = [&](const std::string& s) { o += std::to_string(fnv1a(s.data(), s.size())) + ","; };
  h(system_prompt(true));
  h(system_prompt(false));
  h(simulation_description());
  h(tool_specs().dump());
  for (uint64_t sd : {7ULL, 99ULL, 12345ULL}) {
    Scenario sc = random_scenario(sd);
    o += std::to_string(scenario_hash(sc)) + ",";
    o += std::to_string(simulate(sc, 0).hash()) + ",";
    o += std::to_string(simulate(sc, bug_bit(1) | bug_bit(10)).hash()) + ",";
    h(to_json(sc).dump());
  }
  return o;
}
static void test_rigid2d_pins() {
  std::string got = pin_string();
  const char* want = "10735996641746562028,15931588914492324236,14810643062030168830,13911634046129575570,17059249879344406273,674525538864104921,9591784889939524908,17059249879344406273,1912743565877773604,5151511153684636756,8710745316820828267,1912743565877773604,16313155137686225922,12623240308662354969,13798852851292246572,16313155137686225922,";
  CHECK(got == want);
}

static void test_orbit_domain() {
  // determinism
  Scenario a = random_orbit_scenario(5);
  CHECK(validate(a).empty());
  CHECK(simulate(a, 0).hash() == simulate(a, 0).hash());
  CHECK(scenario_from_json(to_json(a)) == a);
  // clean simulator: no violations on random scenarios
  int clean_flags = 0;
  for (int i = 0; i < 150; ++i) {
    Scenario s = random_orbit_scenario(1000 + static_cast<uint64_t>(i));
    if (!check_rules(s, simulate(s, 0)).empty()) ++clean_flags;
  }
  CHECK(clean_flags == 0);
  // each seeded bug is detectable by random search and shrinks to a smaller reproducing scenario
  for (int b = 0; b < kNumOrbitBugs; ++b) {
    BugMask m = 1u << b;
    bool found = false;
    for (int i = 0; i < 1200 && !found; ++i) {
      Scenario s = random_orbit_scenario(50000 + static_cast<uint64_t>(i));
      auto v = check_rules(s, simulate(s, m));
      if (v.empty()) continue;
      found = true;
      CHECK(check_rules(s, simulate(s, 0)).empty());
      ShrinkResult sr = shrink(s, m, v.front().rule);
      CHECK(sr.ok);
      CHECK(validate(sr.scenario).empty());
      CHECK(sr.size_after <= sr.size_before);
      bool again = false;
      for (const auto& x : check_rules(sr.scenario, simulate(sr.scenario, m))) again |= x.rule == v.front().rule;
      CHECK(again);
    }
    CHECK(found);
  }
  // tools in the orbit domain
  ToolSession ts(0u, ToolBudget(), "/tmp");
  ts.set_domain("orbit");
  Json args = Json::object();
  args["scenario"] = to_json(a);
  Json r = ts.call("run_scenario", args);
  CHECK(r.at("ok").boolean());
  Json q = Json::object();
  q["scenario_id"] = r.at("scenario_id");
  q["step"] = 3;
  Json st = ts.call("inspect_state", q);
  CHECK(st.at("ok").boolean() && st.has("angular_momentum") && st.at("bodies").arr().size() == a.bodies.size());
  Json cr = ts.call("check_rules", q);
  CHECK(cr.at("rules").arr().size() == rule_catalog("orbit").size());
  q["from"] = 0; q["to"] = 5;
  CHECK(ts.call("render_frames", q).at("ok").boolean());
  // the orbit prompt names no seeded bug
  std::string all = system_prompt(true, "orbit") + tool_specs("orbit").dump();
  for (int b = 0; b < kNumOrbitBugs; ++b) CHECK(all.find(orbit_bug_id(b)) == std::string::npos);
  CHECK(all.find("restitution") == std::string::npos && all.find("controller") == std::string::npos);
}

static void test_cartpole_domain() {
  Scenario a = random_cartpole_scenario(5);
  CHECK(validate(a).empty());
  CHECK(simulate(a, 0).hash() == simulate(a, 0).hash());
  CHECK(scenario_from_json(to_json(a)) == a);
  int clean_flags = 0;
  for (int i = 0; i < 200; ++i) {
    Scenario s = random_cartpole_scenario(1000 + static_cast<uint64_t>(i));
    if (!check_rules(s, simulate(s, 0)).empty()) ++clean_flags;
  }
  CHECK(clean_flags == 0);
  for (int b = 0; b < kNumCartBugs; ++b) {
    BugMask m = 1u << b;
    bool found = false;
    for (int i = 0; i < 3000 && !found; ++i) {
      Scenario s = random_cartpole_scenario(70000 + static_cast<uint64_t>(i));
      if (b == 1) {  // c02 is a needle (about 1 in 2000 random scenarios): use a designed worst case instead
        s = Scenario();
        s.domain = "cartpole"; s.steps = 300; s.dt = 0.01;
        s.cp.mc = 2; s.cp.mp = 0.3; s.cp.l = 1.0; s.cp.g = 12; s.cp.th0 = 0.1; s.cp.w0 = 0.3; s.cp.fmax = 40; s.cp.x0 = 0.5; s.cp.v0 = 0.5;
        s.cp.pushes.push_back({0.5, 0.2, 4});
      }
      auto v = check_rules(s, simulate(s, m));
      if (v.empty()) continue;
      found = true;
      CHECK(check_rules(s, simulate(s, 0)).empty());
      ShrinkResult sr = shrink(s, m, v.front().rule);
      CHECK(sr.ok);
      CHECK(validate(sr.scenario).empty());
      CHECK(sr.size_after <= sr.size_before);
      bool again = false;
      for (const auto& x : check_rules(sr.scenario, simulate(sr.scenario, m))) again |= x.rule == v.front().rule;
      CHECK(again);
    }
    CHECK(found);
  }
  ToolSession ts(0u, ToolBudget(), "/tmp");
  ts.set_domain("cartpole");
  Json args = Json::object();
  args["scenario"] = to_json(a);
  Json r = ts.call("run_scenario", args);
  CHECK(r.at("ok").boolean());
  Json q = Json::object();
  q["scenario_id"] = r.at("scenario_id");
  q["step"] = 3;
  Json st = ts.call("inspect_state", q);
  CHECK(st.at("ok").boolean() && st.has("theta") && st.has("pole_angle_margin"));
  q["from"] = 0; q["to"] = 5;
  CHECK(ts.call("render_frames", q).at("ok").boolean());
  std::string all = system_prompt(true, "cartpole") + tool_specs("cartpole").dump();
  for (int b = 0; b < kNumCartBugs; ++b) CHECK(all.find(cart_bug_id(b)) == std::string::npos);
  CHECK(all.find("restitution") == std::string::npos && all.find("walls") == std::string::npos);
}

static void test_engines_and_differential() {
  // engine specs
  CHECK(make_engine("builtin", "orbit") != nullptr);
  bool threw = false;
  try { make_engine("nonsense", "orbit"); } catch (const std::exception&) { threw = true; }
  CHECK(threw);
  threw = false;
  try { make_engine("builtin:zz9", "orbit"); } catch (const std::exception&) { threw = true; }
  CHECK(threw);
  // a program that dies is reported, not hung or silently accepted
  threw = false;
  try { Scenario s = random_orbit_scenario(3); make_engine("exec:false", "orbit")(s); } catch (const std::exception&) { threw = true; }
  CHECK(threw);
  // differential: identical engines agree; a seeded bug is found with no rules
  SimFn clean = make_engine("builtin", "orbit"), bugged = make_engine("builtin:o02", "orbit");
  int agree_flags = 0, bug_flags = 0;
  for (int i = 0; i < 120; ++i) {
    Scenario s = random_orbit_scenario(9000 + static_cast<uint64_t>(i));
    if (first_divergence(s, clean(s), clean(s), 0.05, nullptr, 1.0)) ++agree_flags;
    if (first_divergence(s, clean(s), bugged(s), 0.05, nullptr)) ++bug_flags;
  }
  CHECK(agree_flags == 0);
  CHECK(bug_flags > 0);
  // the independent external integrator speaks the protocol and agrees with the built-in one over 1 s
  if (std::ifstream("build/naive_orbit").good()) {
    SimFn ext = make_engine("exec:build/naive_orbit 0.004", "orbit");
    int ext_flags = 0;
    for (int i = 0; i < 40; ++i) {
      Scenario s = random_orbit_scenario(9500 + static_cast<uint64_t>(i));
      if (first_divergence(s, clean(s), ext(s), 0.1, nullptr, 1.0)) ++ext_flags;
    }
    CHECK(ext_flags == 0);
  }
  // regression bundle round trip: fails on the buggy engine, passes on the clean one
  for (int i = 0; i < 400; ++i) {
    Scenario s = random_orbit_scenario(100 + static_cast<uint64_t>(i));
    auto v = check_rules(s, bugged(s));
    if (v.empty()) continue;
    ShrinkResult sr = shrink(s, bugged, v.front().rule);
    CHECK(sr.ok);
    Json e = Json::object();
    e["domain"] = "orbit"; e["rule"] = v.front().rule;
    write_bundle("/tmp/rc_test_bundle", sr.scenario, e, "test");
    Scenario back = scenario_from_json(Json::parse(read_file("/tmp/rc_test_bundle/scenario.json")), "orbit");
    CHECK(back == sr.scenario);
    bool fails_bugged = false, fails_clean = false;
    for (const auto& x : check_rules(back, bugged(back))) fails_bugged |= x.rule == v.front().rule;
    for (const auto& x : check_rules(back, clean(back))) fails_clean |= x.rule == v.front().rule;
    CHECK(fails_bugged && !fails_clean);
    break;
  }
}

static void test_agent_v2_counterfactual() {
  struct St { std::string shrunk, cf; int vstep = 0; Json shrunk_scenario; } st;
  MockBackend mock([&st](const ModelRequest& req, int i) -> ModelResponse {
    switch (i) {
      case 0: { Json a = Json::object(); a["scenario"] = to_json(trigger(kBigRadiusBound)); return MockBackend::tool("run_scenario", a, "c0"); }
      case 1: { Json a = Json::object(); a["scenario_id"] = last_tool_json(req).at("scenario_id").str(); return MockBackend::tool("shrink", a, "c1"); }
      case 2: {  // the intervention: same shrunk scenario with a small radius (one field)
        Json j = last_tool_json(req);
        st.shrunk = j.at("scenario_id").str();
        st.vstep = static_cast<int>(j.at("violation").at("step").num());
        Json sc = j.at("scenario");
        sc["bodies"].arr()[0]["r"] = 0.5;
        Json a = Json::object(); a["scenario"] = sc;
        return MockBackend::tool("run_scenario", a, "c2");
      }
      case 3: { st.cf = last_tool_json(req).at("scenario_id").str(); Json a = Json::object(); a["scenario_id"] = st.shrunk; a["step"] = st.vstep; return MockBackend::tool("inspect_state", a, "c3"); }
      case 4: {  // no counterfactual: rejected
        double x = last_tool_json(req).at("bodies").arr()[0].at("x").num();
        return MockBackend::tool("submit_report", spent_report(st.shrunk, st.vstep, x, "Body 0 passes the right wall at step " + std::to_string(st.vstep) + "."), "c4");
      }
      case 5: {  // counterfactual = the shrunk scenario itself: rejected
        Json r = last_tool_json(req);
        CHECK(!r.at("ok").boolean());
        Json a = Json::object(); a["scenario_id"] = st.shrunk; a["step"] = st.vstep;
        return MockBackend::tool("inspect_state", a, "c5");
      }
      case 6: {
        double x = last_tool_json(req).at("bodies").arr()[0].at("x").num();
        Json rep = spent_report(st.shrunk, st.vstep, x, "Body 0 passes the right wall at step " + std::to_string(st.vstep) + ".");
        Json cf = Json::object(); cf["scenario_id"] = st.shrunk;
        rep["counterfactual"] = cf;
        return MockBackend::tool("submit_report", rep, "c6");
      }
      case 7: {
        Json r = last_tool_json(req);
        CHECK(!r.at("ok").boolean());
        double x = 0;
        Json a = Json::object(); a["scenario_id"] = st.shrunk; a["step"] = st.vstep;
        (void)x;
        return MockBackend::tool("inspect_state", a, "c7");
      }
      case 8: {
        double x = last_tool_json(req).at("bodies").arr()[0].at("x").num();
        Json rep = spent_report(st.shrunk, st.vstep, x, "Body 0 passes the right wall at step " + std::to_string(st.vstep) + ".");
        Json cf = Json::object(); cf["scenario_id"] = st.cf; cf["change"] = "radius 1.2 -> 0.5";
        rep["counterfactual"] = cf;
        return MockBackend::tool("submit_report", rep, "c8");
      }
      default:
        return MockBackend::text(R"({"description":"ok","agrees":true,"disagreement":""})");
    }
  });
  BudgetedModel bm(mock, Pricing(), 1.0);
  ToolSession ts(bug_bit(kBigRadiusBound), ToolBudget(), ".");
  AgentConfig cfg;
  cfg.version = 2;
  AgentResult r = run_agent(bm, ts, cfg);
  CHECK(r.stop_reason == "report_accepted");
  CHECK(r.report_valid && r.rejected_reports == 2);
  CHECK(r.counterfactual.is_object());
  CHECK(r.counterfactual.at("changed_paths").arr().size() == 1 && r.counterfactual.at("changed_paths").arr()[0].str() == "bodies[0].r");
  // localization scoring: radius is a trigger field of the seeded bug, speed is not
  Json truth = Json::parse(read_file("data/ground_truth.json"));
  CHECK(counterfactual_localizes(truth, "b10", {"bodies[0].r"}));
  CHECK(!counterfactual_localizes(truth, "b10", {"dt"}));
  CHECK(counterfactual_localizes(truth, "b01", {"walls[2].w"}));
  // the v2 prompt and schema add the counterfactual; v1 stays unchanged (pinned above)
  CHECK(system_prompt(true, "rigid2d", 2).find("counterfactual") != std::string::npos);
  CHECK(system_prompt(true, "rigid2d", 1).find("counterfactual") == std::string::npos);
  // scenario_diff
  Json a = to_json(trigger(kBigRadiusBound)), b = a;
  b["bodies"].arr()[0]["r"] = 0.5;
  CHECK(scenario_diff(a, b).size() == 1);
  CHECK(scenario_diff(a, a).empty());
}

int main() {
  struct T { const char* name; std::function<void()> fn; };
  T tests[] = {
      {"json", test_json}, {"scenario", test_scenario}, {"determinism", test_determinism},
      {"clean sim has no violations", test_clean_sim_has_no_violations}, {"checker", test_checker},
      {"each bug detectable", test_each_bug_is_detectable_and_isolated}, {"shrinker", test_shrinker},
      {"baseline budget", test_baseline_budget}, {"tools", test_tools}, {"render", test_render},
      {"ground truth scoring", test_ground_truth_scoring},
      {"wire format", test_wire_format}, {"live backend refuses without key", test_live_backend_refuses_without_key},
      {"budget stop", test_budget}, {"lifetime ledger", test_lifetime_ledger}, {"citation validation", test_citation_validation},
      {"agent end-to-end (mock)", test_agent_end_to_end_mock}, {"agent model budget stop", test_agent_model_budget_stop},
      {"agent JSON protocol", test_agent_json_protocol_mode}, {"record and replay", test_record_and_replay}, {"rigid2d pins", test_rigid2d_pins}, {"orbit domain", test_orbit_domain}, {"cartpole domain", test_cartpole_domain}, {"engines and differential", test_engines_and_differential}, {"agent v2 counterfactual", test_agent_v2_counterfactual},
  };
  for (auto& t : tests) {
    int before = g_fail;
    t.fn();
    std::cout << (g_fail == before ? "[ ok ] " : "[FAIL] ") << t.name << "\n";
  }
  std::cout << g_checks << " checks, " << g_fail << " failed\n";
  return g_fail ? 1 : 0;
}
