// rc_agent: runs the tester agent against a simulation with operator-chosen hidden bugs.
//   rc_agent --replay CACHE.jsonl [options]          serve recorded responses, no network
//   rc_agent --live --config configs/hosted.json     call the real API (needs the key env var)
// Options: --bugs b01,b05 --budget-usd 5 --max-steps N --max-tool-calls N --max-turns N --out DIR
//          --record CACHE.jsonl (append live exchanges for later replay)
// Without --live, nothing in this program can reach the network.
#include <sys/stat.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "agent/agent.hpp"
#include "eval/ground_truth.hpp"

using namespace rc;

int main(int argc, char** argv) {
  std::string replay, config_path = "configs/hosted.json", out = "runs/latest", record, bugs_arg;
  bool live = false, smoke = false;
  double budget_usd = 5.0;
  ToolBudget tb;
  AgentConfig ac;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string {
      if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); }
      return argv[++i];
    };
    if (a == "--replay") replay = val();
    else if (a == "--live") live = true;
    else if (a == "--smoke") smoke = true;
    else if (a == "--config") config_path = val();
    else if (a == "--bugs") bugs_arg = val();
    else if (a == "--budget-usd") budget_usd = std::atof(val().c_str());
    else if (a == "--max-steps") tb.max_steps = std::atoll(val().c_str());
    else if (a == "--max-tool-calls") tb.max_tool_calls = std::atoi(val().c_str());
    else if (a == "--max-turns") ac.max_turns = std::atoi(val().c_str());
    else if (a == "--reasoning-effort") ac.reasoning_effort = val();
    else if (a == "--out") out = val();
    else if (a == "--record") record = val();
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  if (!smoke && replay.empty() && !live) { std::cerr << "choose --replay CACHE or --live\n"; return 2; }

  if (smoke) {
    if (!live) { std::cerr << "--smoke needs --live\n"; return 2; }
    ApiConfig cfg = ApiConfig::from_json(Json::parse(read_file(config_path)));
    MetaApiBackend be(cfg);
    if (ledger_total_usd(default_ledger_path()) + 0.05 > default_lifetime_cap_usd()) { std::cerr << "lifetime spend cap reached\n"; return 2; }
    auto show = [&](const char* label, const ModelRequest& rq) {
      ModelResponse r = be.complete(rq);
      std::cout << "== " << label << ": ok=" << r.ok << " http=" << r.http_status << " latency_ms=" << r.latency_ms
                << " finish=" << r.finish_reason << "\n   error=" << r.error << "\n   text=" << r.message.content.substr(0, 200)
                << "\n   tool_calls=" << r.message.tool_calls.size() << " usage(in/out/cached/reasoning)=" << r.usage.prompt_tokens
                << "/" << r.usage.completion_tokens << "/" << r.usage.cached_tokens << "/" << r.usage.reasoning_tokens
                << "\n   raw=" << r.raw << "\n";
    };
    ModelRequest a1;
    a1.model = cfg.model; a1.max_tokens = 2000;
    a1.messages.push_back({"user", "Reply with the single word: pong", {}, "", {}});
    show("text", a1);
    ModelRequest a2 = a1;
    a2.messages.clear();
    ChatMessage im{"user", "What colour is this image? One word.", {}, "", {"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=="}};
    a2.messages.push_back(im);
    show("image", a2);
    ModelRequest a3 = a1;
    a3.messages.clear();
    a3.messages.push_back({"user", "Call the echo tool with text 'hi'.", {}, "", {}});
    a3.tools = Json::parse(R"([{"name":"echo","description":"Echo text back","parameters":{"type":"object","properties":{"text":{"type":"string"}},"required":["text"]}}])");
    show("tool", a3);
    ModelRequest a4 = a1;
    a4.reasoning_effort = "low";
    show("text+reasoning_effort=low", a4);
    return 0;
  }

  BugMask mask = 0;
  std::stringstream ss(bugs_arg);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    int idx = bug_index(tok);
    if (idx < 0) { std::cerr << "unknown bug id: " << tok << "\n"; return 2; }
    mask |= bug_bit(idx);
  }
  std::string mk = "mkdir -p '" + out + "'";
  if (std::system(mk.c_str()) != 0) return 1;

  ApiConfig api = ApiConfig::from_json(Json::parse(read_file(config_path)));
  ac.model = api.model;
  ac.native_tools = api.native_tools;
  ac.out_dir = out;

  std::unique_ptr<ModelBackend> backend;
  std::unique_ptr<ModelBackend> inner;
  if (live) {
    inner.reset(new MetaApiBackend(api));
    if (!record.empty()) backend.reset(new RecordingBackend(*inner, record));
    else backend = std::move(inner);
  } else {
    backend.reset(new ReplayBackend(replay));
  }
  BudgetedModel model(*backend, api.pricing, budget_usd, out + "/cost.jsonl");
  if (live) model.set_lifetime_ledger(default_ledger_path(), default_lifetime_cap_usd());
  ToolSession tools(mask, tb, out);
  std::ofstream events(out + "/events.jsonl", std::ios::trunc);
  AgentResult res = run_agent(model, tools, ac, [&](const Json& ev) {
    events << ev.dump() << "\n";
    std::cout << ev.dump().substr(0, 400) << "\n";
  });
  write_report_files(res, tools, out);
  Usage u = model.total_usage();
  std::cout << "stop=" << res.stop_reason << " found=" << res.found_failure << " report_valid=" << res.report_valid
            << " calls=" << model.calls() << " tokens_in=" << u.prompt_tokens << " tokens_out=" << u.completion_tokens
            << " cost_usd=" << model.spent() << " (budget " << model.budget() << ")\n";
  return 0;
}
