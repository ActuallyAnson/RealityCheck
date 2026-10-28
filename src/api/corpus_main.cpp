// rc_corpus: run the externally sourced scenarios in data/external_scenarios through the declared rules on several engines.
//   rc_corpus [--dir data/external_scenarios] [--rebound /path/to/rebound_orbit]
// Prints a markdown table: per scenario and engine, the violated rules (with first step) or the largest rule margin (1.0 = at the limit).
#include <dirent.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include "api/api.hpp"

using namespace rc;

static std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
static std::vector<std::string> list(const std::string& d, bool dirs) {
  std::vector<std::string> out;
  if (DIR* dp = opendir(d.c_str())) {
    while (dirent* e = readdir(dp)) {
      std::string n = e->d_name;
      if (n[0] == '.') continue;
      bool isjson = n.size() > 5 && n.substr(n.size() - 5) == ".json";
      if (dirs ? !isjson : isjson) out.push_back(n);
    }
    closedir(dp);
  }
  std::sort(out.begin(), out.end());
  return out;
}

int main(int argc, char** argv) {
  std::string dir = "data/external_scenarios", rebound;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--dir" && i + 1 < argc) dir = argv[++i];
    else if (a == "--rebound" && i + 1 < argc) rebound = argv[++i];
    else { std::cerr << "usage: rc_corpus [--dir D] [--rebound PATH]\n"; return 2; }
  }
  ApiOptions opt;
  opt.allow_exec = true;  // the operator running this tool chose the engines below
  std::cout << "| domain | scenario | engine | result |\n|---|---|---|---|\n";
  for (const std::string& domain : list(dir, true)) {
    std::vector<std::string> engines = {"builtin"};
    if (domain == "rigid2d") { engines.push_back("box2d"); engines.push_back("box2d-noccd"); }
    if (domain == "orbit" && !rebound.empty()) for (const char* s : {"ias15 1 0", "bs", "leapfrog 4", "leapfrog 1"}) engines.push_back("exec:" + rebound + " " + s);
    for (const std::string& f : list(dir + "/" + domain, false)) {
      Json sc = Json::parse(slurp(dir + "/" + domain + "/" + f));
      for (const std::string& eng : engines) {
        Json a = Json::object();
        a["domain"] = domain; a["engine"] = eng; a["scenario"] = sc;
        Json r = api_call("run", a, opt);
        std::string res;
        if (!r.bool_or("ok", false)) res = "n/a (" + r.str_or("error", "") + ")";
        else if (r.at("violation_count").num() > 0) {
          for (const Json& v : r.at("violations").arr()) res += (res.empty() ? "" : ", ") + v.str_or("rule", "") + "@" + std::to_string(static_cast<int>(v.num_or("step", 0)));
          res = "VIOLATES " + res;
        } else {
          double mx = 0;
          for (auto& kv : r.at("rule_margins").obj()) mx = std::max(mx, kv.second.num());
          char b[48]; std::snprintf(b, sizeof b, "passes (largest margin %.2f)", mx); res = b;
        }
        std::string label = eng;
        if (eng.compare(0, 5, "exec:") == 0) { size_t sp = eng.find(' '); label = "rebound " + eng.substr(sp + 1); }
        std::cout << "| " << domain << " | " << f.substr(0, f.size() - 5) << " | " << label << " | " << res << " |\n";
      }
    }
  }
  return 0;
}
