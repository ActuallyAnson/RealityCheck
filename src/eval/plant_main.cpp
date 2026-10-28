// rc_plant: a model plants realistic bugs in a simulator's source, so the bug set is not chosen by the author.
//   rc_plant --live --file src/sim/orbit.cpp --out /tmp/planted/orbit --count 8 [--config configs/hosted_1_2.json] [--budget-usd 0.5]
// The model sees only the source (no rules, no oracles, no tests). It answers with single-line edits {line, old, new}; the tool applies them mechanically,
// refuses anything that adds a preprocessor line or touches the operating system, and writes one self-contained .cpp per bug plus planted.json (descriptions,
// kept out of the evaluation until the set is locked: scripts/blind_lock.sh, then scripts/blind_eval.sh; see docs/BLIND.md).
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "model/model.hpp"
#include "util/json.hpp"

using namespace rc;

namespace {
std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
std::vector<std::string> split_lines(const std::string& s) { std::vector<std::string> v; std::stringstream ss(s); for (std::string l; std::getline(ss, l);) v.push_back(l); return v; }
bool denied(const std::string& s) {
  static const char* bad[] = {"#", "system", "popen", "exec", "fork", "fopen", "freopen", "ofstream", "ifstream", "fstream", "filesystem", "socket", "asm", "dlopen", "getenv", "setenv", "rand(", "time(", "clock("};
  for (const char* b : bad) if (s.find(b) != std::string::npos) return true;
  return false;
}
std::string json_object_from(const std::string& text) {
  size_t a = text.find('{');
  if (a == std::string::npos) return "";
  int depth = 0;
  bool in_str = false;
  for (size_t i = a; i < text.size(); ++i) {
    char c = text[i];
    if (in_str) { if (c == '\\') ++i; else if (c == '"') in_str = false; continue; }
    if (c == '"') in_str = true; else if (c == '{') ++depth; else if (c == '}' && --depth == 0) return text.substr(a, i - a + 1);
  }
  return "";
}
}  // namespace

int main(int argc, char** argv) {
  std::string file, out, config = "configs/hosted_1_2.json";
  int count = 8;
  double budget = 0.5;
  bool live = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--file") file = val(); else if (a == "--out") out = val(); else if (a == "--count") count = std::atoi(val().c_str()); else if (a == "--config") config = val();
    else if (a == "--budget-usd") budget = std::atof(val().c_str()); else if (a == "--live") live = true; else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  if (!live || file.empty() || out.empty()) { std::cerr << "usage: rc_plant --live --file SRC.cpp --out DIR [--count N] [--config C] [--budget-usd X]\n"; return 2; }
  if (std::system(("mkdir -p '" + out + "'").c_str()) != 0) return 1;
  ApiConfig api = ApiConfig::from_json(Json::parse(slurp(config)));
  MetaApiBackend backend(api);
  RecordingBackend rec(backend, out + "/cache.jsonl");
  BudgetedModel model(rec, api.pricing, budget, out + "/cost.jsonl");
  model.set_lifetime_ledger(default_ledger_path(), default_lifetime_cap_usd());
  const std::vector<std::string> src = split_lines(slurp(file));
  std::ostringstream p;
  p << "You are helping evaluate bug-finding tools. Below is the C++ source of a small deterministic simulator. Plant " << count << " DIFFERENT, realistic bugs in it, the kind a "
       "developer could plausibly introduce by mistake (a wrong sign, an off-by-one, a wrong constant or unit, a missing term, a swapped argument, a wrong comparison, a bug that only appears in "
       "an unusual situation, a numerical-precision mistake). Mix the kinds: some that affect every run, some that appear only under particular conditions. Each bug must be independent "
       "(applied alone to the original source), must still compile, and must change behavior in some scenarios. Do not add comments that reveal the change.\n\n"
       "Answer with one JSON object and nothing else: {\"bugs\":[{\"id\":\"p01\",\"description\":\"one sentence\",\"edits\":[{\"line\":N,\"old\":\"exact text on that line\",\"new\":\"replacement text\"}]}]}. "
       "Use 1 to 3 edits per bug, each edit replaces the exact substring `old` (which must occur on line N, counted from 1) with `new`. Do not add #include or #define lines and do not touch the operating system.\n\nSOURCE:\n";
  for (size_t i = 0; i < src.size(); ++i) p << (i + 1) << ": " << src[i] << "\n";
  std::vector<ChatMessage> msgs;
  msgs.push_back({"user", p.str(), {}, "", {}});
  ModelRequest rq; rq.model = api.model; rq.max_tokens = 12000; rq.reasoning_effort = "low"; rq.messages = msgs;
  ModelResponse resp;
  try { resp = model.complete(rq, "plant"); } catch (const BudgetExceeded&) { std::cerr << "budget\n"; return 1; }
  if (!resp.ok) { std::cerr << "model error: " << resp.error << "\n"; return 1; }
  Json j;
  try { j = Json::parse(json_object_from(resp.message.content)); } catch (const std::exception& e) { std::cerr << "unparsed answer\n"; return 1; }
  Json kept = Json::array();
  int refused = 0, applied = 0;
  for (const Json& b : j.at("bugs").arr()) {
    std::vector<std::string> v = src;
    bool ok = true;
    for (const Json& e : b.at("edits").arr()) {
      int line = static_cast<int>(e.num_or("line", 0));
      std::string o = e.str_or("old", ""), n = e.str_or("new", "");
      if (line < 1 || line > static_cast<int>(v.size()) || o.empty() || o == n || denied(n)) { ok = false; break; }
      size_t pos = v[static_cast<size_t>(line - 1)].find(o);
      if (pos == std::string::npos) { ok = false; break; }
      v[static_cast<size_t>(line - 1)].replace(pos, o.size(), n);
    }
    if (!ok) { ++refused; continue; }
    std::string id = b.str_or("id", "p" + std::to_string(applied + 1));
    std::ofstream f(out + "/" + id + ".cpp");
    for (const std::string& l : v) f << l << "\n";
    Json k = Json::object(); k["id"] = id; k["description"] = b.str_or("description", ""); k["edits"] = b.at("edits");
    kept.push(k);
    ++applied;
  }
  Json o = Json::object(); o["source"] = file; o["model"] = api.model; o["planted"] = kept; o["refused_edits"] = refused;
  std::ofstream(out + "/planted.json") << o.dump() << "\n";
  std::cout << "wrote " << applied << " variants (" << refused << " refused as unappliable or unsafe) to " << out << "; spent $" << model.spent() << "\n";
  return 0;
}
