// rc: one front door for the RealityCheck tools. It runs the matching binary that sits next to it, passing the rest of the command line through.
//   rc <command> [args...]      rc help      rc doctor
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "model/model.hpp"
#ifdef RC_HAVE_BOX2D
#define RC_BOX2D_NOTE "yes"
#else
#define RC_BOX2D_NOTE "no (brew install box2d, then reconfigure)"
#endif

namespace {
struct Cmd { const char* name; const char* binary; const char* what; };
const Cmd kCmds[] = {
    {"hunt", "rc_hunt", "random hunt on any engine, with regression bundles"},
    {"agent", "rc_agent", "the model-driven tester (needs MODEL_API_KEY and --live)"},
    {"triage", "rc_triage", "fuzzer finds, agent explains: the triage evaluation"},
    {"spec", "rc_spec", "the model writes invariants from a description; the engine validates them"},
    {"repro", "rc_repro", "export or re-run a regression test"},
    {"diff", "rc_diff", "differential test of two engines"},
    {"eval", "rc_eval", "locked evaluations: baseline, agent, summarize, export, spend, compare"},
    {"corpus", "rc_corpus", "run the externally sourced scenarios"},
    {"api", "rc_api_server", "the deterministic tools as a local HTTP API (or --mcp)"},
    {"serve", "rc_demo", "the viewer at http://127.0.0.1:8080"},
    {"tool", "rc_tool", "call the deterministic tools directly"},
    {"fuzz", "rc_fuzz", "fuzz the parsers and the API (build with sanitizers)"},
    {"climb", "rc_climb", "margin-guided hill climber vs random (no model)"},
    {"mutgen", "rc_mutgen", "generate source mutants of a simulator"},
    {"mutscore", "rc_mutscore", "score mutation-benchmark results"},
};

std::string dir_of(const char* argv0) {
  std::string s = argv0;
  size_t p = s.find_last_of('/');
  return p == std::string::npos ? "." : s.substr(0, p);
}
bool exists(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0; }

void usage() {
  std::cout << "rc: RealityCheck, a verification harness for simulations (a model proposes, a program decides)\n\nusage: rc <command> [args]\n\ncommands:\n";
  for (const Cmd& c : kCmds) { std::string n = c.name; n.resize(9, ' '); std::cout << "  " << n << c.what << "\n"; }
  std::cout << "  spend     lifetime API spend against the cap\n  doctor    check this installation\n  help      this text\n\n"
               "Run `rc <command> --help` for a command's options. Domains: rigid2d, orbit, cartpole, traffic, and generic (set RC_MODEL_CARD; docs/GENERIC_DOMAIN.md).\n";
}

int doctor(const std::string& dir) {
  int problems = 0;
  std::cout << "rc doctor\n";
  for (const Cmd& c : kCmds) {
    bool ok = exists(dir + "/" + c.binary);
    std::cout << "  " << (ok ? "[ok]   " : "[miss] ") << c.binary << "\n";
    if (!ok && std::string(c.name) != "fuzz" && std::string(c.name) != "mutgen" && std::string(c.name) != "mutscore" && std::string(c.name) != "climb") ++problems;
  }
  const char* key = std::getenv("MODEL_API_KEY");
  std::cout << "  " << (key && *key ? "[ok]   " : "[note] ") << "MODEL_API_KEY is " << (key && *key ? "set" : "not set (needed only for --live runs)") << "\n";
  std::string ledger = rc::default_ledger_path();
  std::cout << "  [ok]   spend ledger " << ledger << ": $" << rc::ledger_total_usd(ledger) << " of the $" << rc::default_lifetime_cap_usd() << " cap\n";
  std::cout << "  [info] Box2D engine built in: " << RC_BOX2D_NOTE << "\n";
  const char* card = std::getenv("RC_MODEL_CARD");
  std::cout << "  [info] RC_MODEL_CARD: " << (card && *card ? card : "not set (needed only for --domain generic)") << "\n";
  std::cout << (problems ? "some tools are missing: build them with `cmake --build build -j`\n" : "everything needed for the built-in domains is present\n");
  return problems ? 1 : 0;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || !std::strcmp(argv[1], "help") || !std::strcmp(argv[1], "--help") || !std::strcmp(argv[1], "-h")) { usage(); return argc < 2 ? 2 : 0; }
  const std::string dir = dir_of(argv[0]), sub = argv[1];
  if (sub == "doctor") return doctor(dir);
  std::string binary, extra;
  if (sub == "spend") { binary = "rc_eval"; extra = "spend"; }
  for (const Cmd& c : kCmds) if (sub == c.name) binary = c.binary;
  if (binary.empty()) { std::cerr << "rc: unknown command '" << sub << "' (try `rc help`)\n"; return 2; }
  std::string path = dir + "/" + binary;
  if (!exists(path)) { std::cerr << "rc: " << path << " is not built (cmake --build build -j)\n"; return 2; }
  std::vector<char*> args;
  args.push_back(const_cast<char*>(path.c_str()));
  if (!extra.empty()) args.push_back(const_cast<char*>(extra.c_str()));
  for (int i = 2; i < argc; ++i) args.push_back(argv[i]);
  args.push_back(nullptr);
  execv(path.c_str(), args.data());
  std::cerr << "rc: cannot run " << path << "\n";
  return 2;
}
