// rc_refgen: a model writes an independent one-step reference implementation of a simulator from its plain-language description alone
// (no source, no declared rules, no bug information), as an oracle for rc_refcheck / scripts/refcheck_worker.sh. docs/REFERENCE_ORACLE.md.
//   rc_refgen --live --domain orbit|cartpole|traffic --out DIR [--conventions] [--config C] [--budget-usd X]
// The model may revise only after a compile error. It never sees a clean-simulator counterexample, so the reference stays independent of the system under test.
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "model/model.hpp"
#include "tools/tools.hpp"
#include "util/json.hpp"

using namespace rc;

namespace {
std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
std::string fenced(const std::string& text) {
  size_t a = text.find("```");
  if (a == std::string::npos) return "";
  a = text.find('\n', a);
  size_t b = text.find("```", a);
  if (a == std::string::npos || b == std::string::npos) return "";
  return text.substr(a + 1, b - a - 1);
}
std::string denied(const std::string& s) {
  static const char* bad[] = {"#", "system", "popen", "exec", "fork", "fopen", "freopen", "ofstream", "ifstream", "fstream", "filesystem", "socket", "asm", "dlopen", "getenv", "setenv",
                              "printf", "cout", "cerr", "cin", "stdout", "stderr", "stdin", "FILE", "write(", "read(", "main("};
  for (const char* b : bad) if (s.find(b) != std::string::npos) return b;
  return "";
}
}  // namespace

int main(int argc, char** argv) {
  std::string domain = "orbit", out, config = "configs/hosted.json";
  bool live = false, conventions = false;
  double budget = 0.5;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--domain") domain = val(); else if (a == "--out") out = val(); else if (a == "--config") config = val(); else if (a == "--live") live = true;
    else if (a == "--conventions") conventions = true; else if (a == "--budget-usd") budget = std::atof(val().c_str()); else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  if (!live || out.empty() || (domain != "orbit" && domain != "cartpole" && domain != "traffic")) { std::cerr << "usage: rc_refgen --live --domain orbit|cartpole|traffic --out DIR [--conventions]\n"; return 2; }
  if (std::system(("mkdir -p '" + out + "'").c_str()) != 0) return 1;
  ApiConfig api = ApiConfig::from_json(Json::parse(slurp(config)));
  MetaApiBackend backend(api);
  RecordingBackend rec(backend, out + "/cache.jsonl");
  BudgetedModel model(rec, api.pricing, budget, out + "/cost.jsonl");
  model.set_lifetime_ledger(default_ledger_path(), default_lifetime_cap_usd());

  const bool traffic = domain == "traffic";
  std::string desc = traffic ? slurp("src/eval/ref_traffic_spec.txt") : simulation_description(domain);
  size_t cut = desc.find("DECLARED RULES");
  if (cut != std::string::npos) desc = desc.substr(0, cut);  // the model is not shown the rules
  const std::string decl = slurp("src/eval/ref_" + domain + "_decl.hpp");
  std::ostringstream p;
  p << "You are writing an independent reference implementation of a simulator, to be used as a test oracle, from its plain-language description alone. You cannot see the simulator's source.\n\n"
    << desc << "\nWrite the body of one function that advances the state by " << (traffic ? "ONE second" : "ONE outer step of length `dt`") << ". The declarations below are already provided (do not repeat them):\n```\n" << decl << "```\n";
  if (traffic) p << "Requirements: follow the specification exactly. Only <cmath>, <algorithm> and <vector> are available; no input or output, no #include or #define lines, no global mutable state, deterministic.\n";
  else p << "Requirements: use the physics exactly as the description states it; be as numerically accurate as you can (use your own sub-stepping or a higher-order method; you do not have to copy the simulator's scheme, a more accurate answer is better). "
            "Only <cmath>, <algorithm> and <vector> are available; no input or output, no #include or #define lines, no global mutable state, deterministic.\n";
  if (conventions) p << "Conventions: positive theta tilts the pole toward +x, the direction in which a positive force pushes the cart; x, v, th, w are the cart position, cart velocity, pole angle and pole angular rate.\n";
  p << "Answer with the complete function definition `" << (traffic ? "void ref_traffic_second(const TrafficParams& p, TrafficRefState* st, const TrafficSecond& in, TrafficRefOut* out)" : std::string("void ref_step_") + domain + "(const " + (domain == "orbit" ? "OrbitIn& in, OrbitOut* out" : "CartIn& in, CartOut* out") + ")")
    << " { ... }` (helper functions allowed) in one fenced code block and nothing else.";
  std::vector<ChatMessage> msgs;
  msgs.push_back({"user", p.str(), {}, "", {}});
  std::string code;
  Json log = Json::array();
  for (int attempt = 1; attempt <= 3; ++attempt) {
    ModelRequest rq; rq.model = api.model; rq.max_tokens = 12000; rq.reasoning_effort = "low"; rq.messages = msgs;
    ModelResponse resp;
    try { resp = model.complete(rq, "refgen"); } catch (const BudgetExceeded&) { std::cerr << "budget\n"; return 1; }
    if (!resp.ok) { std::cerr << "model error: " << resp.error << "\n"; return 1; }
    msgs.push_back(resp.message);
    code = fenced(resp.message.content);
    std::string feedback;
    if (code.empty()) feedback = "No fenced code block found.";
    else if (!denied(code).empty()) feedback = "Refused: the code contains '" + denied(code) + "'. Plain numeric code only.";
    else {
      std::ofstream(out + "/ref_" + domain + ".cpp") << code;
      std::string abs = out;
      char* rp = realpath((out + "/ref_" + domain + ".cpp").c_str(), nullptr);
      if (rp) { abs = rp; free(rp); }
      std::string cmd = "c++ -std=c++17 -O2 -w -Isrc -DREF_" + std::string(domain == "orbit" ? "ORBIT" : traffic ? "TRAFFIC" : "CARTPOLE") + " -DREF_IMPL='\"" + abs + "\"' src/eval/refrun_main.cpp -o '" + out + "/refrun_" + domain + "' 2> '" + out + "/compile.log'";
      if (std::system(cmd.c_str()) == 0) { Json l = Json::object(); l["attempt"] = attempt; l["result"] = "compiled"; log.push(l); break; }
      std::string first; { std::stringstream ss(slurp(out + "/compile.log")); for (std::string ln; std::getline(ss, ln) && first.size() < 600;) if (ln.find("error") != std::string::npos) first += ln + "\n"; }
      feedback = "It did not compile:\n" + first;
    }
    Json l = Json::object(); l["attempt"] = attempt; l["result"] = feedback; log.push(l);
    code.clear();
    msgs.push_back({"user", feedback + "\nFix only that and answer again with the complete function in one fenced code block.", {}, "", {}});
  }
  std::ofstream(out + "/refgen.json") << log.dump() << "\n";
  if (code.empty()) { std::cout << "no compiling reference after 3 attempts\n"; return 1; }
  std::cout << "reference written to " << out << "/ref_" << domain << ".cpp; compiled runner " << out << "/refrun_" << domain << "; spent $" << model.spent() << "\n";
  return 0;
}
