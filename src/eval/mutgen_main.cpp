// rc_mutgen: mechanical mutation of a simulator source file (no knowledge of any seeded bug).
//   rc_mutgen --file src/sim/orbit.cpp --ranges 74-147,164-189 --count 40 --seed 1 --out /tmp/mut/orbit
// Lists every mutation site (operator or constant) on the given line ranges, draws `count` of them uniformly with the seed, and writes
// one mutated copy of the file per site (m000.cpp, ...) plus index.json (line, original, mutated). Sites that do not compile are
// found later, when the mutant is built, and are reported there.
#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

#include "util/json.hpp"
#include "util/rng.hpp"

using namespace rc;

struct Site { int line; size_t pos, len; std::string repl, desc; };

static std::vector<std::pair<int, int>> parse_ranges(const std::string& s) {
  std::vector<std::pair<int, int>> out;
  std::stringstream ss(s);
  std::string tok;
  while (std::getline(ss, tok, ',')) { size_t d = tok.find('-'); out.push_back({std::atoi(tok.substr(0, d).c_str()), std::atoi(tok.substr(d + 1).c_str())}); }
  return out;
}

int main(int argc, char** argv) {
  std::string file, ranges, out = "mut";
  int count = 40, order = 1;  // order: simultaneous mutations per mutant (on distinct lines); 1 reproduces the locked benchmark
  uint64_t seed = 1;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--file") file = val(); else if (a == "--ranges") ranges = val(); else if (a == "--count") count = std::atoi(val().c_str());
    else if (a == "--seed") seed = static_cast<uint64_t>(std::atoll(val().c_str())); else if (a == "--out") out = val(); else if (a == "--order") order = std::atoi(val().c_str());
    else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  std::ifstream f(file);
  if (!f || ranges.empty()) { std::cerr << "usage: rc_mutgen --file F --ranges a-b,c-d [--count N] [--seed S] [--out DIR]\n"; return 2; }
  std::vector<std::string> lines;
  for (std::string l; std::getline(f, l);) lines.push_back(l);
  const std::vector<std::pair<std::string, std::string>> ops = {{" <= ", " < "}, {" < ", " <= "}, {" >= ", " > "}, {" > ", " >= "}, {" == ", " != "}, {" != ", " == "},
      {" && ", " || "}, {" || ", " && "}, {" + ", " - "}, {" - ", " + "}, {" * ", " / "}, {" / ", " * "}, {" += ", " -= "}, {" -= ", " += "}};
  std::regex dbl(R"((^|[^\w.\[])(\d+\.\d+(?:e[+-]?\d+)?)(?![\w.]))"), integer(R"((^|[^\w.\[])(\d+)(?![\w.\]]))");
  std::vector<Site> sites;
  for (auto& r : parse_ranges(ranges)) {
    for (int ln = r.first; ln <= r.second && ln <= static_cast<int>(lines.size()); ++ln) {
      std::string code = lines[static_cast<size_t>(ln - 1)];
      size_t c = code.find("//");
      if (c != std::string::npos) code = code.substr(0, c);
      size_t first = code.find_first_not_of(" \t");
      if (first == std::string::npos || code[first] == '#' || code.find("template") != std::string::npos || code.find("<<") != std::string::npos ||
          code.find('"') != std::string::npos || code.find("bugs & ") != std::string::npos || code.find("bugs&") != std::string::npos ||
          code.find("kOrbitBug") != std::string::npos || code.find("kCartBug") != std::string::npos || code.find("kTraf") != std::string::npos ||
          code.find("bug_bit") != std::string::npos || code.find("has(") != std::string::npos)
        continue;  // preprocessor, templates, streams, strings, and the seeded-bug switches (dead code on the clean path)
      for (auto& op : ops)
        for (size_t p = code.find(op.first); p != std::string::npos; p = code.find(op.first, p + 1))
          sites.push_back({ln, p, op.first.size(), op.second, "operator '" + op.first.substr(1, op.first.size() - 2) + "' -> '" + op.second.substr(1, op.second.size() - 2) + "'"});
      for (auto it = std::sregex_iterator(code.begin(), code.end(), dbl); it != std::sregex_iterator(); ++it) {
        size_t p = static_cast<size_t>(it->position(2)); std::string tok = (*it)[2];
        sites.push_back({ln, p, tok.size(), "(" + tok + " * 1.1)", "constant " + tok + " -> " + tok + " * 1.1"});
      }
      for (auto it = std::sregex_iterator(code.begin(), code.end(), integer); it != std::sregex_iterator(); ++it) {
        size_t p = static_cast<size_t>(it->position(2)); std::string tok = (*it)[2];
        if (tok.size() > 6) continue;
        sites.push_back({ln, p, tok.size(), std::to_string(std::atoi(tok.c_str()) + 1), "constant " + tok + " -> " + std::to_string(std::atoi(tok.c_str()) + 1)});
      }
    }
  }
  std::cout << sites.size() << " mutation sites in " << file << "\n";
  Rng rng(seed);
  for (size_t i = sites.size(); i > 1; --i) std::swap(sites[i - 1], sites[static_cast<size_t>(rng.next() % i)]);
  if (std::system(("mkdir -p '" + out + "'").c_str()) != 0) return 1;
  Json index = Json::array();
  int n = std::min<int>(count, static_cast<int>(sites.size() / static_cast<size_t>(std::max(order, 1))));
  size_t next = 0;
  for (int k = 0; k < n; ++k) {
    std::vector<const Site*> pick;
    if (order <= 1) pick.push_back(&sites[static_cast<size_t>(k)]);
    else {  // the next `order` sites in the shuffled list that sit on distinct lines
      for (; next < sites.size() && static_cast<int>(pick.size()) < order; ++next) {
        bool dup = false;
        for (const Site* q : pick) dup |= q->line == sites[next].line;
        if (!dup) pick.push_back(&sites[next]);
      }
      if (static_cast<int>(pick.size()) < order) { n = k; break; }
    }
    std::vector<std::string> m = lines;
    std::string desc;
    for (const Site* sp : pick) {
      std::string& l = m[static_cast<size_t>(sp->line - 1)];
      l.replace(sp->pos, sp->len, sp->repl);
      desc += (desc.empty() ? "" : " + ") + sp->desc;
    }
    const Site& s = *pick[0];
    char name[16];
    std::snprintf(name, sizeof name, "m%03d.cpp", k);
    std::ofstream(out + "/" + name) << [&] { std::string t; for (auto& x : m) t += x + "\n"; return t; }();
    Json o = Json::object();
    o["id"] = std::string(name).substr(0, 4); o["line"] = s.line; o["change"] = desc; o["source_line"] = lines[static_cast<size_t>(s.line - 1)];
    if (order > 1) { Json ls = Json::array(); for (const Site* sp : pick) ls.push(sp->line); o["lines"] = ls; }
    index.push(o);
  }
  std::ofstream(out + "/index.json") << index.dump() << "\n";
  std::cout << "wrote " << n << " mutants to " << out << "\n";
  return 0;
}
