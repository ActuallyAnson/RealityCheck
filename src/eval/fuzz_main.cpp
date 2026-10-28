// rc_fuzz: a small mutation fuzzer for the input-facing code (JSON parser, scenario parsers and validators, invariant parser, API operations).
// Run it under AddressSanitizer and UBSan (see docs/ROBUSTNESS.md). A crash, a sanitizer report or a hang is a finding; thrown std::exceptions are the expected rejection path.
//   rc_fuzz [--iterations 200000] [--api-iterations 1500] [--seed 1]
#include <signal.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>

#include "api/api.hpp"
#include "sim/domains.hpp"
#include "spec/spec.hpp"
#include "util/rng.hpp"

using namespace rc;

namespace {
const char* kNasty[] = {"null", "true", "false", "0", "-1", "1e999", "-1e999", "1e-999", "123456789012345678901234567890", "\"x\"", "[]", "{}", "[[[]]]", "{\"a\":{\"a\":{}}}",
                        "NaN", "Infinity", "0.0000000001", "4294967296", "-4294967296", "\"\"", "[null]", "{\"\":1}"};

std::string mutate(std::string s, Rng& rng) {
  int rounds = 1 + static_cast<int>(rng.next() % 4);
  for (int r = 0; r < rounds && !s.empty(); ++r) {
    size_t p = static_cast<size_t>(rng.next() % s.size());
    switch (rng.next() % 7) {
      case 0: s[p] = static_cast<char>(rng.next() & 0xff); break;                       // flip a byte
      case 1: s.erase(p, 1 + rng.next() % 8); break;                                     // delete
      case 2: s.insert(p, s.substr(p, 1 + rng.next() % 16)); break;                      // duplicate a run
      case 3: { size_t q = p; while (q < s.size() && (std::isdigit(static_cast<unsigned char>(s[q])) || s[q] == '.' || s[q] == '-' || s[q] == 'e')) ++q;  // replace a number
                if (q > p) s.replace(p, q - p, kNasty[rng.next() % (sizeof kNasty / sizeof *kNasty)]); break; }
      case 4: s.insert(p, kNasty[rng.next() % (sizeof kNasty / sizeof *kNasty)]); break;
      case 5: s.resize(p); break;                                                        // truncate
      default: { size_t q = static_cast<size_t>(rng.next() % s.size()); std::swap(s[p], s[q]); }
    }
  }
  return s;
}

void on_alarm(int) { std::cerr << "HANG: an iteration ran longer than 60 s\n"; std::_Exit(3); }
}  // namespace

int main(int argc, char** argv) {
  long iterations = 200000, api_iterations = 1500;
  uint64_t seed = 1;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--iterations" && i + 1 < argc) iterations = std::atol(argv[++i]);
    else if (a == "--api-iterations" && i + 1 < argc) api_iterations = std::atol(argv[++i]);
    else if (a == "--seed" && i + 1 < argc) seed = static_cast<uint64_t>(std::atoll(argv[++i]));
    else { std::cerr << "usage: rc_fuzz [--iterations N] [--api-iterations N] [--seed S]\n"; return 2; }
  }
  signal(SIGALRM, on_alarm);
  const char* domains[] = {"rigid2d", "orbit", "cartpole", "traffic"};
  std::vector<std::pair<std::string, std::string>> seeds;  // (domain, valid scenario JSON text)
  for (const char* d : domains) for (uint64_t k = 1; k <= 6; ++k) seeds.push_back({d, to_json(random_scenario_for(d, k)).dump()});
  std::vector<std::string> inv_seeds = {
      R"([{"id":"a","kind":"conserved","signal":"energy","abs_tol":0.1,"rel_tol":0.01},{"id":"b","kind":"bound","signal":"max_speed","op":"<=","value":10},{"id":"c","kind":"exclusive","signal":"state_N","signal_b":"state_E"},{"id":"d","kind":"min_run","signal":"state_N","equals":1,"steps":3}])"};
  Rng rng(seed);
  long parsed_ok = 0, rejected = 0;
  for (long it = 0; it < iterations; ++it) {
    alarm(60);
    int target = static_cast<int>(rng.next() % 3);
    if (target == 0) {  // JSON parser on arbitrary mutated text
      std::string text = mutate(seeds[rng.next() % seeds.size()].second, rng);
      try { Json j = Json::parse(text); (void)j.dump(); ++parsed_ok; } catch (const std::exception&) { ++rejected; }
    } else if (target == 1) {  // scenario parser + validator + (when valid and small) the simulator
      auto& sd = seeds[rng.next() % seeds.size()];
      std::string text = mutate(sd.second, rng);
      try {
        Scenario sc = scenario_from_json(Json::parse(text), sd.first);
        std::string why = validate(sc);
        if (why.empty() && sc.steps <= 400) { Trace tr = simulate(sc, 0); (void)check_rules(sc, tr); (void)extract_signals(sc, tr); }
        ++parsed_ok;
      } catch (const std::exception&) { ++rejected; }
    } else {  // invariant parser
      std::string text = mutate(inv_seeds[0], rng);
      std::vector<std::string> pr;
      (void)parse_invariants(text, domains[rng.next() % 4], &pr, 1 + static_cast<int>(rng.next() % 2));
    }
  }
  ApiOptions opt;
  const char* ops[] = {"run", "shrink", "bundle", "hunt", "random_scenario", "domains"};
  for (long it = 0; it < api_iterations; ++it) {
    alarm(60);
    auto& sd = seeds[rng.next() % seeds.size()];
    std::string text = mutate(sd.second, rng);
    Json args = Json::object();
    try { args["scenario"] = Json::parse(text); } catch (const std::exception&) { continue; }
    args["domain"] = rng.next() % 8 == 0 ? std::string("nope") : sd.first;
    args["engine"] = rng.next() % 3 == 0 ? std::string("builtin:b01,b02") : std::string("builtin");
    args["seed"] = static_cast<double>(rng.next() % 1000);
    args["budget_steps"] = static_cast<double>(rng.next() % 5000);
    std::string op = ops[rng.next() % 6];
    if (op == "bundle" || op == "shrink" || op == "hunt") { Scenario probe; try { probe = scenario_from_json(args.at("scenario"), sd.first); if (probe.steps > 600) continue; } catch (const std::exception&) {} }
    Json r = api_call(op, args, opt);
    if (!r.has("ok")) { std::cerr << "API result without ok for op " << op << "\n"; return 1; }
  }
  std::cout << "fuzzed " << iterations << " parser iterations (" << parsed_ok << " accepted, " << rejected << " rejected) and " << api_iterations << " API calls: no crash, no hang, no sanitizer report\n";
  return 0;
}
