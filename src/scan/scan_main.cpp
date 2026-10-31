// rc_scan: point it at YOUR simulator, no model card to write and no API key. It probes the program for its signals, builds a model card from the
// parameter ranges you give, learns what "normal" looks like from the program's own runs (mined invariants), then hunts scenarios, including
// boundary values, for: failed runs, non-finite signals, nondeterminism, and runs that break a mined invariant. Each finding is minimized by
// resetting parameters to their defaults while it still fails, and written to a short report plus a card that every other tool accepts.
//
//   rc_scan --cmd ./mysim --param gain:0.1:8:log --param capacity:5:60:int --choice mode:proportional,bang_bang --steps 200 --out scan_out
//
// The program speaks the JSON-lines protocol of docs/PROTOCOL.md (generic domain: docs/GENERIC_DOMAIN.md). Mined invariants are candidates learned
// from the program itself: a bug present in every run looks normal to them, so a clean scan is not a certificate (docs/MINING.md).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "external/engine.hpp"
#include "sim/domains.hpp"
#include "sim/generic.hpp"
#include "spec/miner.hpp"
#include "util/rng.hpp"

using namespace rc;

namespace {
std::vector<std::string> split(const std::string& s, char c) {
  std::vector<std::string> v;
  std::stringstream ss(s);
  for (std::string t; std::getline(ss, t, c);) v.push_back(t);
  return v;
}

// One probe request with default parameters; returns the signal names the program reports.
std::string shell_quote(const std::string& t) {
  std::string o = "'";
  for (char c : t) { if (c == '\'') o += "'\\''"; else o += c; }
  return o + "'";
}

// Runs a shell command with stdin from /dev/null, collects stdout (stderr discarded) and kills its process group after `limit` seconds.
std::string run_capture(const std::string& command, double limit, bool* timed_out, int* exit_code) {
  *timed_out = false; *exit_code = -1;
  int fd[2];
  if (pipe(fd) != 0) return "";
  const pid_t pid = fork();
  if (pid == 0) {
    setpgid(0, 0);
    dup2(fd[1], 1);
    const int dn = open("/dev/null", O_RDWR);
    if (dn >= 0) { dup2(dn, 0); dup2(dn, 2); }
    close(fd[0]); close(fd[1]);
    execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }
  close(fd[1]);
  std::string out;
  const auto t0 = std::chrono::steady_clock::now();
  char buf[65536];
  for (;;) {
    const double left = limit - std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (left <= 0) { *timed_out = true; break; }
    pollfd pf = {fd[0], POLLIN, 0};
    const int r = poll(&pf, 1, static_cast<int>(left * 1000) + 1);
    if (r == 0) continue;
    if (r < 0) break;
    const ssize_t n = read(fd[0], buf, sizeof buf);
    if (n <= 0) break;
    out.append(buf, static_cast<size_t>(n));
  }
  if (*timed_out) { kill(-pid, SIGKILL); kill(pid, SIGKILL); }
  int st = 0;
  waitpid(pid, &st, 0);
  if (WIFEXITED(st)) *exit_code = WEXITSTATUS(st);
  close(fd[0]);
  return out;
}

// Adapter-free mode (`--cmd "./sim --gain {gain} --steps {steps}"`): rc_scan runs itself as `rc_scan --shim TEMPLATE`, which speaks the JSON-lines protocol
// and, per request, fills the {name} placeholders (parameters, {steps}, {seed}), runs the command, and reads a table from its stdout: a header line with
// the signal names (separated by commas, tabs or spaces) and one numeric row per step (steps + 1 rows, step 0 first).
int shim_main(const std::string& tmpl) {
  const char* env = std::getenv("RC_EXEC_TIMEOUT_S");
  const double limit = env && std::atof(env) > 0 ? std::atof(env) : 60.0;
  std::string line;
  while (std::getline(std::cin, line)) {
    Json out = Json::object();
    try {
      const Json req = Json::parse(line);
      const Json& sc = req.at("scenario");
      std::map<std::string, std::string> val;
      if (sc.has("params"))
        for (const auto& kv : sc.at("params").obj()) {
          if (kv.second.is_number()) { char b[64]; std::snprintf(b, sizeof b, "%.17g", kv.second.num()); val[kv.first] = b; }
          else val[kv.first] = kv.second.str();
        }
      { char b[64]; std::snprintf(b, sizeof b, "%d", static_cast<int>(sc.num_or("steps", 200))); val["steps"] = b; std::snprintf(b, sizeof b, "%.0f", sc.num_or("seed", 1)); val["seed"] = b; }
      std::string cmd;
      for (size_t i = 0; i < tmpl.size(); ++i) {
        if (tmpl[i] == '{') {
          const size_t e = tmpl.find('}', i);
          if (e != std::string::npos) {
            const std::string k = tmpl.substr(i + 1, e - i - 1);
            if (!val.count(k)) throw std::runtime_error("the command template uses {" + k + "} but there is no parameter, choice, {steps} or {seed} of that name");
            cmd += val[k]; i = e; continue;
          }
        }
        cmd += tmpl[i];
      }
      bool to = false; int ec = 0;
      const std::string text = run_capture(cmd, limit, &to, &ec);
      if (to) throw std::runtime_error("the command did not finish within " + std::to_string(static_cast<int>(limit)) + " s and was killed (a hang, not a finding)");
      if (ec != 0) throw std::runtime_error("the command exited with status " + std::to_string(ec) + ": " + cmd.substr(0, 160));
      std::vector<std::string> rows = split(text, '\n');
      while (!rows.empty() && rows.back().find_first_not_of(" \t\r") == std::string::npos) rows.pop_back();
      if (rows.size() < 2) throw std::runtime_error("the command printed fewer than 2 lines; expected a header of signal names and one numeric row per step");
      auto cells = [](std::string r) { for (char& c : r) if (c == ',' || c == '\t' || c == '\r') c = ' '; std::vector<std::string> v; std::stringstream ss(r); for (std::string t; ss >> t;) v.push_back(t); return v; };
      const std::vector<std::string> names = cells(rows[0]);
      std::vector<Json> cols(names.size(), Json::array());
      for (size_t r = 1; r < rows.size(); ++r) {
        const std::vector<std::string> c = cells(rows[r]);
        if (c.size() != names.size()) throw std::runtime_error("row " + std::to_string(r) + " has " + std::to_string(c.size()) + " values, the header has " + std::to_string(names.size()) + " names");
        for (size_t k = 0; k < c.size(); ++k) {
          char* endp = nullptr; const double v = std::strtod(c[k].c_str(), &endp);
          if (endp == c[k].c_str() || *endp) { if (c[k] == "nan" || c[k] == "NaN" || c[k] == "inf" || c[k] == "-inf" || c[k] == "Infinity") cols[k].push(std::nan("")); else throw std::runtime_error("row " + std::to_string(r) + ": '" + c[k] + "' is not a number"); }
          else cols[k].push(v);
        }
      }
      Json sig = Json::object();
      for (size_t k = 0; k < names.size(); ++k) sig[names[k]] = cols[k];
      out["ok"] = true; out["signals"] = sig;
    } catch (const std::exception& e) { out["ok"] = false; out["error"] = e.what(); }
    std::cout << out.dump() << "\n" << std::flush;
  }
  return 0;
}

std::vector<std::string> probe_signals(const std::string& cmd, const Json& params, int steps) {
  Json sc = Json::object();
  sc["domain"] = "generic"; sc["seed"] = 1; sc["steps"] = steps; sc["params"] = params;
  Json req = Json::object();
  req["op"] = "simulate"; req["scenario"] = sc;
  const std::string in = "/tmp/rc_scan_probe_" + std::to_string(::getpid()) + ".json";
  std::ofstream(in) << req.dump() << "\n";
  // Run the program in its own process group and wait for its first output line for at most RC_EXEC_TIMEOUT_S seconds (default 60, as the exec engine does),
  // so a program that never answers on the default parameters ends the scan with a message instead of stalling it.
  const char* env = std::getenv("RC_EXEC_TIMEOUT_S");
  const double limit = env && std::atof(env) > 0 ? std::atof(env) : 60.0;
  std::string line;
  bool timed_out = false;
  int fd[2];
  if (pipe(fd) == 0) {
    const std::string sh = "( " + cmd + " ) < '" + in + "'";
    const pid_t pid = fork();
    if (pid == 0) { setpgid(0, 0); dup2(fd[1], 1); dup2(fd[1], 2); close(fd[0]); close(fd[1]); execl("/bin/sh", "sh", "-c", sh.c_str(), static_cast<char*>(nullptr)); _exit(127); }
    close(fd[1]);
    const auto t0 = std::chrono::steady_clock::now();
    char c;
    for (;;) {
      const double left = limit - std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      if (left <= 0) { timed_out = true; break; }
      pollfd pf = {fd[0], POLLIN, 0};
      const int r = poll(&pf, 1, static_cast<int>(left * 1000) + 1);
      if (r == 0) continue;
      if (r < 0) break;
      if (read(fd[0], &c, 1) <= 0) break;
      if (c == '\n') break;
      line += c;
    }
    if (pid > 0) { kill(-pid, SIGKILL); kill(pid, SIGKILL); waitpid(pid, nullptr, 0); }
    close(fd[0]);
  }
  std::remove(in.c_str());
  if (timed_out) throw std::runtime_error("the program did not answer the probe request within " + std::to_string(static_cast<int>(limit)) + " s (RC_EXEC_TIMEOUT_S); it hangs on the default parameters, which is itself worth a look");
  if (line.empty()) throw std::runtime_error("the program printed nothing for the probe request; it must read one JSON line from stdin and answer with one (docs/PROTOCOL.md)");
  Json r;
  try { r = Json::parse(line); } catch (const std::exception&) { throw std::runtime_error("the program's first output line is not JSON: " + line.substr(0, 160)); }
  if (!r.has("ok") || !r.at("ok").boolean()) throw std::runtime_error("the program answered the probe with an error: " + line.substr(0, 200));
  std::vector<std::string> names;
  for (const auto& kv : r.at("signals").obj()) names.push_back(kv.first);
  if (names.empty()) throw std::runtime_error("the answer has no \"signals\" (name -> array of steps+1 numbers)");
  return names;
}

struct Finding {
  std::string kind, detail;
  int count = 0;
  Scenario example;
  std::string why;                // plain-language reason computed for the smallest example
  Signals sig;                    // signals of the smallest example (for the HTML plot)
  std::string plot_signal;        // signal the finding is about, when it is about one
  bool has_limit = false;
  double limit = 0;
  int at_step = -1;
};

std::string num(double v) { std::ostringstream o; o << v; return o.str(); }

std::string html_escape(const std::string& t) {
  std::string o;
  for (char c : t) { if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;"; else o += c; }
  return o;
}

// One small SVG line chart of a signal; the limit (when given) is a dashed line and the step where a finding starts is a vertical mark.
std::string svg_plot(const std::string& name, const std::vector<double>& y, bool hot, bool has_limit, double limit, int at_step) {
  const double W = 360, H = 120, L = 8, R = 8, T = 18, B = 8;
  double lo = 1e300, hi = -1e300;
  for (double v : y) if (std::isfinite(v)) { lo = std::min(lo, v); hi = std::max(hi, v); }
  if (has_limit && std::isfinite(limit)) { lo = std::min(lo, limit); hi = std::max(hi, limit); }
  if (!(lo <= hi)) { lo = 0; hi = 1; }
  if (hi - lo < 1e-12) { lo -= 0.5; hi += 0.5; }
  const size_t n = y.size();
  auto X = [&](size_t i) { return L + (W - L - R) * (n > 1 ? static_cast<double>(i) / static_cast<double>(n - 1) : 0.0); };
  auto Y = [&](double v) { return T + (H - T - B) * (1.0 - (v - lo) / (hi - lo)); };
  std::ostringstream o;
  o << "<svg viewBox=\"0 0 " << W << " " << H << "\" role=\"img\" aria-label=\"" << html_escape(name) << "\"><text x=\"" << L << "\" y=\"12\" class=\"lab" << (hot ? " hot" : "") << "\">" << html_escape(name)
    << "  [" << num(lo) << " .. " << num(hi) << "]</text>";
  if (has_limit && std::isfinite(limit)) o << "<line x1=\"" << L << "\" x2=\"" << W - R << "\" y1=\"" << Y(limit) << "\" y2=\"" << Y(limit) << "\" class=\"lim\"/>";
  if (at_step >= 0 && hot) o << "<line x1=\"" << X(static_cast<size_t>(at_step)) << "\" x2=\"" << X(static_cast<size_t>(at_step)) << "\" y1=\"" << T << "\" y2=\"" << H - B << "\" class=\"mark\"/>";
  std::string pts;
  bool open = false;
  for (size_t i = 0; i < n; ++i) {
    if (!std::isfinite(y[i])) { if (open) { o << "<polyline class=\"ln" << (hot ? " hot" : "") << "\" points=\"" << pts << "\"/>"; pts.clear(); open = false; } continue; }
    pts += num(X(i)) + "," + num(Y(y[i])) + " "; open = true;
  }
  if (open) o << "<polyline class=\"ln" << (hot ? " hot" : "") << "\" points=\"" << pts << "\"/>";
  o << "</svg>";
  return o.str();
}

std::string describe(const GenericModel& m, const Scenario& sc, bool all = false) {
  std::ostringstream o;
  o << "steps=" << sc.steps;
  for (const GenericParam& g : m.params) {
    const double v = sc.gen.params.count(g.name) ? sc.gen.params.at(g.name) : g.def;
    if (!all && std::fabs(v - g.def) < 1e-12 * (1 + std::fabs(g.def))) continue;
    if (g.type == "choice") o << ", " << g.name << "=" << g.choices[static_cast<size_t>(v)];
    else o << ", " << g.name << "=" << v;
  }
  return o.str();
}
}  // namespace

int main(int argc, char** argv) {
  if (argc >= 3 && std::string(argv[1]) == "--shim") return shim_main(argv[2]);
  std::string cmd, out = "scan_out", name = "my simulator";
  int steps = 200, n = 400, pool_n = 200;
  std::vector<std::string> requires_;  // --require "signal<=value" / "signal>=value": rules you state yourself
  bool absolute = false;  // --absolute-bounds: keep mined absolute bounds on signals that follow the start (the older, noisier behavior)
  Json params = Json::array();
  std::vector<std::pair<std::string, Json>> defs;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
    if (a == "--cmd") cmd = val(); else if (a == "--out") out = val(); else if (a == "--name") name = val();
    else if (a == "--absolute-bounds") absolute = true;
    else if (a == "--require") requires_.push_back(val());
    else if (a == "--steps") steps = std::atoi(val().c_str()); else if (a == "--scenarios") n = std::atoi(val().c_str());
    else if (a == "--param" || a == "--choice") {
      const std::vector<std::string> f = split(val(), a == "--param" ? ':' : ':');
      Json p = Json::object();
      p["name"] = f.at(0);
      if (a == "--choice") {
        Json ch = Json::array();
        for (const std::string& c : split(f.at(1), ',')) ch.push(c);
        p["type"] = "choice"; p["choices"] = ch; p["default"] = split(f.at(1), ',').at(0);
      } else {
        if (f.size() < 3) { std::cerr << "--param NAME:MIN:MAX[:int|log]\n"; return 2; }
        const double lo = std::atof(f[1].c_str()), hi = std::atof(f[2].c_str());
        const bool is_int = f.size() > 3 && f[3] == "int", is_log = f.size() > 3 && f[3] == "log";
        p["type"] = is_int ? "int" : "float"; p["min"] = lo; p["max"] = hi;
        if (is_log) { p["scale"] = "log"; p["default"] = std::sqrt(lo * hi); } else p["default"] = is_int ? std::floor((lo + hi) / 2) : (lo + hi) / 2;
      }
      params.push(p);
    } else { std::cerr << "unknown option " << a << "\n"; return 2; }
  }
  if (cmd.empty() || params.arr().empty()) {
    std::cerr << "usage: rc_scan --cmd PROGRAM|COMMAND-WITH-{param}-PLACEHOLDERS --param NAME:MIN:MAX[:int|log] [--param ...] [--choice NAME:a,b,c] [--steps N] [--scenarios N] [--out DIR] [--name TEXT] [--absolute-bounds] [--require signal<=value]\n";
    return 2;
  }
  const std::string shown_cmd = cmd;
  if (cmd.find('{') != std::string::npos) {  // command template: no adapter needed (see shim_main)
    char self[4096];
    const std::string me = realpath(argv[0], self) ? std::string(self) : std::string(argv[0]);
    cmd = shell_quote(me) + " --shim " + shell_quote(cmd);
  }
  try {
    Json defaults = Json::object();
    for (const Json& p : params.arr()) defaults[p.at("name").str()] = p.at("default");
    const std::vector<std::string> sigs = probe_signals(cmd, defaults, std::min(steps, 20));
    Json card = Json::object();
    card["name"] = name;
    card["description"] = "Simulator under scan: " + name + ". Signals: " + [&] { std::string s; for (const auto& x : sigs) s += (s.empty() ? "" : ", ") + x; return s; }();
    card["command"] = cmd;
    card["parameters"] = params;
    Json st = Json::object(); st["min"] = std::min(steps, 20); st["max"] = steps; st["default"] = steps; card["steps"] = st;
    Json sj = Json::array();
    for (const std::string& s : sigs) { Json o = Json::object(); o["name"] = s; o["text"] = s; sj.push(o); }
    card["signals"] = sj;
    card["rules"] = Json::array();
    set_generic_model(parse_generic_model(card));
    const GenericModel& m = generic_model();
    std::system(("mkdir -p '" + out + "'").c_str());
    std::ofstream(out + "/card.json") << card.dump() << "\n";
    std::printf("probed %s: %zu signals (%s), %zu parameters\n", shown_cmd.c_str(), sigs.size(), card.at("description").str().substr(card.at("description").str().find("Signals: ") + 9).c_str(), m.params.size());

    SimFn sim = make_engine("card", "generic");
    auto run = [&](const Scenario& sc, Signals* sg, std::string* err) {
      try { Trace tr = sim(sc); *sg = extract_signals(sc, tr); return true; }
      catch (const std::exception& e) { *err = e.what(); return false; }
    };
    // Boundary-aware scenario: each parameter is pushed to its range edge with probability 0.3 (docs/HYBRID.md: seeded bugs often trigger at edges).
    auto scenario = [&](uint64_t seed, bool edges) {
      Scenario sc = random_generic_scenario(seed);
      sc.steps = std::min(sc.steps, steps);
      if (edges) {
        Rng r(seed * 7919 + 13);
        for (const GenericParam& g : m.params) {
          if (g.type == "choice" || r.range(0, 1) > 0.3) continue;
          sc.gen.params[g.name] = r.range(0, 1) < 0.5 ? g.min : g.max;
        }
      }
      return sc;
    };

    std::vector<Signals> pool;
    std::vector<std::map<std::string, double>> pool_params;
    for (int i = 0; i < pool_n; ++i) {
      Signals sg; std::string err;
      const Scenario psc = scenario(900000 + static_cast<uint64_t>(i), false);
      if (run(psc, &sg, &err)) { pool.push_back(sg); pool_params.push_back(psc.gen.params); }
    }
    if (pool.empty()) throw std::runtime_error("every probe run failed; check the program on a default scenario");
    std::vector<Invariant> inv = mine_invariants(pool);
    if (!absolute) {
      // A signal whose value at step 0 already varies a lot between runs follows the starting conditions (momentum of a random start, energy of a random
      // layout); an absolute bound on it only restates the largest start seen, and fires on any valid input with a bigger start. Replace such bounds by one
      // relative to the run's own start: |x(k) - x(0)| <= R * (|x(0)| + 5% of the signal's scale), R = twice the largest ratio seen in the normal runs, at least 1e-6.
      for (const auto& kv : pool[0]) {
        const std::string& sn = kv.first;
        double x0lo = 1e300, x0hi = -1e300, lo = 1e300, hi = -1e300, scale = 0;
        for (const Signals& sg : pool) { auto it = sg.find(sn); if (it == sg.end() || it->second.empty()) continue; x0lo = std::min(x0lo, it->second[0]); x0hi = std::max(x0hi, it->second[0]);
          for (double v : it->second) if (std::isfinite(v)) { lo = std::min(lo, v); hi = std::max(hi, v); scale = std::max(scale, std::fabs(v)); } }
        if (!(hi > lo) || !(x0hi - x0lo > 0.25 * (hi - lo))) continue;
        double ratio = 0;
        for (const Signals& sg : pool) { auto it = sg.find(sn); if (it == sg.end() || it->second.empty()) continue;
          for (double v : it->second) if (std::isfinite(v)) ratio = std::max(ratio, std::fabs(v - it->second[0]) / (std::fabs(it->second[0]) + 0.05 * scale)); }
        std::vector<Invariant> keep;
        for (const Invariant& v : inv) if (!(v.kind == "bound" && v.signal == sn)) keep.push_back(v);
        inv = keep;
        Invariant c; c.id = "rel_" + sn; c.kind = "conserved"; c.signal = sn; const double R = std::max(2 * ratio, 1e-6);  // below 1e-6 relative a change is floating-point noise, not behavior
        c.rel_tol = R; c.abs_tol = R * 0.05 * scale;
        c.text = sn + " stays within " + num(c.rel_tol) + " x (|its start| + " + num(0.05 * scale) + ") of its own start (mined, relative)";
        inv.push_back(c);
      }
    }
    std::printf("learned %zu candidate invariants from %zu normal runs\n", inv.size(), pool.size());

    // Scale-aware limits: when the largest |value| of a signal grows or shrinks with a positive numeric parameter (gravity, launch speed, frequency) the
    // normal runs show it, and a fixed bound only restates the biggest parameter seen. For such a signal the bounds are replaced by a log-log fit
    // log max|S| = a + b log P with a margin of twice the largest residual (at least a factor 3). The fit is tried on all runs and on the runs of each
    // value of each choice parameter (a dependence that only holds inside one model or mode, with at least 25 runs and correlation 0.8).
    struct Scaled { std::string sig, param, text, cond; double condv = -1; bool has_cond = false; double a = 0, b = 0, margin = 0; };
    std::vector<Scaled> scaled;
    if (!absolute && pool.size() >= 30) {
      auto fit = [&](const std::string& sn, const std::vector<size_t>& idx, std::string* bp, double* ba, double* bb, double* bres) {
        std::vector<double> y;
        for (size_t i : idx) { double mx = 0; auto it = pool[i].find(sn); if (it != pool[i].end()) for (double v : it->second) if (std::isfinite(v)) mx = std::max(mx, std::fabs(v)); y.push_back(mx); }
        for (double v : y) if (!(v > 0)) return false;
        double best = 0;
        for (const GenericParam& g : m.params) {
          if (g.type == "choice" || !(g.min > 0) || g.max < 4 * g.min) continue;
          std::vector<double> x, ly;
          for (size_t k = 0; k < idx.size(); ++k) { const auto& pp = pool_params[idx[k]]; x.push_back(std::log(pp.count(g.name) ? pp.at(g.name) : g.def)); ly.push_back(std::log(y[k])); }
          const double n_ = static_cast<double>(x.size());
          double mx = 0, my = 0; for (size_t i = 0; i < x.size(); ++i) { mx += x[i]; my += ly[i]; } mx /= n_; my /= n_;
          double sxx = 0, syy = 0, sxy = 0; for (size_t i = 0; i < x.size(); ++i) { sxx += (x[i] - mx) * (x[i] - mx); syy += (ly[i] - my) * (ly[i] - my); sxy += (x[i] - mx) * (ly[i] - my); }
          if (sxx <= 0 || syy <= 0) continue;
          const double rho = sxy / std::sqrt(sxx * syy), b = sxy / sxx, a = my - b * mx;
          if (std::fabs(rho) < 0.8 || std::fabs(rho) <= std::fabs(best)) continue;
          double res = 0; for (size_t i = 0; i < x.size(); ++i) res = std::max(res, std::fabs(ly[i] - (a + b * x[i])));
          best = rho; *bp = g.name; *ba = a; *bb = b; *bres = res;
        }
        return best != 0;
      };
      std::vector<size_t> all(pool.size());
      for (size_t i = 0; i < all.size(); ++i) all[i] = i;
      for (const auto& kv : pool[0]) {
        const std::string& sn = kv.first;
        std::string bp; double ba = 0, bb = 0, bres = 0;
        if (fit(sn, all, &bp, &ba, &bb, &bres)) {
          std::vector<Invariant> keep;
          for (const Invariant& v : inv) if (!(v.kind == "bound" && v.signal == sn)) keep.push_back(v);
          inv = keep;
          Scaled sd; sd.sig = sn; sd.param = bp; sd.a = ba; sd.b = bb; sd.margin = std::max(std::log(3.0), 2 * bres);
          sd.text = "max |" + sn + "| follows " + bp + "^" + num(bb) + " within a factor " + num(std::exp(sd.margin)) + " (mined, scaled)";
          scaled.push_back(sd);
          continue;
        }
        for (const GenericParam& c : m.params) {
          if (c.type != "choice") continue;
          for (size_t cv = 0; cv < c.choices.size(); ++cv) {
            std::vector<size_t> sub;
            for (size_t i = 0; i < pool.size(); ++i) { auto f = pool_params[i].find(c.name); if (f != pool_params[i].end() && static_cast<size_t>(f->second + 0.5) == cv) sub.push_back(i); }
            if (sub.size() < 25 || !fit(sn, sub, &bp, &ba, &bb, &bres)) continue;
            Scaled sd; sd.sig = sn; sd.param = bp; sd.a = ba; sd.b = bb; sd.margin = std::max(std::log(3.0), 2 * bres); sd.cond = c.name; sd.condv = static_cast<double>(cv); sd.has_cond = true;
            sd.text = "when " + c.name + " = " + c.choices[cv] + ", max |" + sn + "| follows " + bp + "^" + num(bb) + " within a factor " + num(std::exp(sd.margin)) + " (mined, scaled)";
            scaled.push_back(sd);
          }
        }
      }
    }
    // Signals that never changed in any normal run: any change is worth a look.
    if (!absolute)
      for (const auto& kv : pool[0]) {
        const std::string& sn = kv.first;
        double lo = 1e300, hi = -1e300;
        for (const Signals& sg : pool) { auto it = sg.find(sn); if (it != sg.end()) for (double v : it->second) if (std::isfinite(v)) { lo = std::min(lo, v); hi = std::max(hi, v); } }
        if (!(hi == lo) || pool.size() < 20) continue;
        for (const char* op : {"<=", ">="}) {
          Invariant c; c.id = std::string("const_") + sn + op; c.kind = "bound"; c.signal = sn; c.op = op; c.has_value = true; c.value = lo;
          c.text = sn + " " + op + " " + num(lo) + " (it was exactly " + num(lo) + " in every normal run; mined, constant)";
          inv.push_back(c);
        }
      }
    for (const std::string& r : requires_) {
      const size_t at = r.find("<=") != std::string::npos ? r.find("<=") : r.find(">=");
      if (at == std::string::npos) { std::cerr << "rc_scan: --require needs signal<=value or signal>=value, got '" << r << "'\n"; return 2; }
      Invariant c; c.id = "require" + std::to_string(inv.size()); c.kind = "bound"; c.signal = r.substr(0, at); c.op = r.substr(at, 2); c.has_value = true; c.value = std::atof(r.substr(at + 2).c_str());
      if (!pool[0].count(c.signal)) { std::cerr << "rc_scan: --require: the program has no signal '" << c.signal << "'\n"; return 2; }
      c.text = "required by you: " + c.signal + " " + c.op + " " + num(c.value);
      inv.push_back(c);
    }

    std::map<std::string, Finding> found;
    auto record = [&](const std::string& key, const std::string& kind, const std::string& detail, const Scenario& sc) {
      Finding& f = found[key];
      if (f.count++ == 0) { f.kind = kind; f.detail = detail; f.example = sc; }
    };
    auto classify = [&](const Scenario& sc, std::vector<std::pair<std::string, std::string>>* hits) {
      Signals sg, sg2; std::string err;
      if (!run(sc, &sg, &err)) { hits->push_back({"run failed", "the program failed or answered an error: " + err.substr(0, 160)}); return; }
      for (const auto& kv : sg)
        for (size_t k = 0; k < kv.second.size(); ++k)
          if (!std::isfinite(kv.second[k])) { hits->push_back({"non-finite signal " + kv.first, "signal " + kv.first + " is not finite at step " + std::to_string(k)}); goto done_nan; }
      done_nan:
      if (run(sc, &sg2, &err) && sg2 != sg) {
        bool same = true;
        for (const auto& kv : sg) { const auto& o = sg2.at(kv.first); for (size_t k = 0; k < kv.second.size() && same; ++k) if (!(kv.second[k] == o[k] || (std::isnan(kv.second[k]) && std::isnan(o[k])))) same = false; }
        if (!same) hits->push_back({"nondeterministic", "the same scenario gives different signals on a second run"});
      }
      auto covered = [&](const Scaled& sd) { return !sd.has_cond || (sc.gen.params.count(sd.cond) && static_cast<int>(sc.gen.params.at(sd.cond) + 0.5) == static_cast<int>(sd.condv)); };
      for (const Scaled& sd : scaled) {
        if (!covered(sd)) continue;
        auto it = sg.find(sd.sig); if (it == sg.end()) continue;
        double mxv = 0; for (double v : it->second) if (std::isfinite(v)) mxv = std::max(mxv, std::fabs(v));
        const double pv = sc.gen.params.count(sd.param) ? sc.gen.params.at(sd.param) : 1.0;
        if (!(pv > 0) || !(mxv > 0)) continue;
        const double pred = sd.a + sd.b * std::log(pv);
        if (std::log(mxv) > pred + sd.margin) hits->push_back({"breaks learned invariant: " + sd.text, sd.text + " (observed " + num(mxv) + " vs about " + num(std::exp(pred)) + " expected from " + sd.param + "=" + num(pv) + ")"});
      }
      for (const Invariant& v : inv) { bool skip = false; for (const Scaled& sd : scaled) if (sd.has_cond && sd.sig == v.signal && covered(sd)) skip = true; if (skip) continue; InvFail f = check_invariant(v, sg); if (f.failed) hits->push_back({"breaks learned invariant: " + v.text, v.text + " (observed " + std::to_string(f.observed) + " vs limit " + std::to_string(f.limit) + " at step " + std::to_string(f.step) + ")"}); }
    };
    for (int i = 0; i < n; ++i) {
      Scenario sc = scenario(700000 + static_cast<uint64_t>(i), i % 2 == 1);
      std::vector<std::pair<std::string, std::string>> hits;
      classify(sc, &hits);
      for (const auto& h : hits) record(h.first, h.first.substr(0, 3) == "run" ? "failure" : h.first.substr(0, 3) == "non" ? "numeric" : h.first.substr(0, 3) == "bre" ? "invariant" : "determinism", h.second, sc);
    }
    // Minimize each finding: reset parameters to their defaults while the same finding still appears.
    for (auto& kv : found) {
      Scenario sc = kv.second.example;
      auto still = [&](const Scenario& s) { std::vector<std::pair<std::string, std::string>> h; classify(s, &h); for (const auto& x : h) if (x.first == kv.first) return true; return false; };
      for (const GenericParam& g : m.params) { Scenario t = sc; t.gen.params[g.name] = g.def; if (still(t)) sc = t; }
      { Scenario t = sc; t.steps = std::max(m.steps_min, sc.steps / 2); if (t.steps != sc.steps && still(t)) sc = t; }
      kv.second.example = sc;
      // Why: which parameters this finding depends on (tried at the range ends, the default, a choice's every option and the current value), and the final detail.
      std::vector<std::string> dep;
      for (const GenericParam& g : m.params) {
        std::vector<double> cands;
        if (g.type == "choice") for (size_t c = 0; c < g.choices.size(); ++c) cands.push_back(static_cast<double>(c));
        else cands = {g.min, g.def, g.max, sc.gen.params.count(g.name) ? sc.gen.params.at(g.name) : g.def};
        std::vector<double> uniq;
        for (double c : cands) { bool seen = false; for (double u : uniq) if (std::fabs(u - c) < 1e-12 * (1 + std::fabs(c))) seen = true; if (!seen) uniq.push_back(c); }
        std::string holds, gone;
        for (double c : uniq) {
          Scenario t = sc; t.gen.params[g.name] = c;
          const std::string label = g.type == "choice" ? g.choices[static_cast<size_t>(c)] : num(c);
          if (still(t)) holds += (holds.empty() ? "" : " or ") + label; else gone += (gone.empty() ? "" : ", ") + label;
        }
        if (!gone.empty()) dep.push_back(g.name + " = " + holds + " (it disappears at " + gone + ")");
      }
      kv.second.why = dep.empty() ? "No single parameter change among the values tried (range ends, default, every choice) makes it disappear, so it does not hinge on one parameter."
                                  : "It depends on: " + [&] { std::string j; for (size_t i = 0; i < dep.size(); ++i) j += (i ? "; " : "") + dep[i]; return j; }() + ".";
      std::vector<std::pair<std::string, std::string>> h2; classify(sc, &h2);
      for (const auto& x : h2) if (x.first == kv.first) kv.second.detail = x.second;
      std::string err2; run(sc, &kv.second.sig, &err2);
      for (const Invariant& v : inv)
        if (kv.first == "breaks learned invariant: " + v.text && kv.second.sig.count(v.signal)) {
          InvFail f = check_invariant(v, kv.second.sig);
          kv.second.plot_signal = v.signal; kv.second.has_limit = true; kv.second.limit = f.limit; kv.second.at_step = f.step;
        }
      for (const Scaled& sd : scaled) if (kv.first == "breaks learned invariant: " + sd.text) kv.second.plot_signal = sd.sig;
    }

    // Order: hard failures first (hang or error, non-finite, nondeterminism), then rules you stated, then learned guesses; ties by how many scenarios hit.
    auto rank = [](const std::string& key) { return key.rfind("breaks learned invariant: required by you", 0) == 0 ? 1 : key.rfind("breaks learned invariant", 0) == 0 ? 2 : 0; };
    auto label = [&](const std::string& key) { const int r = rank(key); return r == 0 ? "HARD FAILURE" : r == 1 ? "your rule" : "learned guess"; };
    std::vector<std::pair<std::string, Finding>> ordered(found.begin(), found.end());
    std::stable_sort(ordered.begin(), ordered.end(), [&](const auto& a, const auto& b) { return rank(a.first) != rank(b.first) ? rank(a.first) < rank(b.first) : a.second.count > b.second.count; });
    std::ostringstream rep;
    rep << "# Scan of " << name << "\n\n" << n << " scenarios (half pushed to parameter boundaries), " << pool.size() << " normal runs mined, " << inv.size() << " candidate invariants.\n\n";
    if (found.empty()) rep << "No finding. This is not a certificate: a bug present in every run looks normal to learned invariants (docs/MINING.md). Add your own rules to `card.json` (docs/GENERIC_DOMAIN.md) or review the learned invariants.\n";
    else rep << "| type | finding | scenarios | smallest example (values left at their default are omitted) |\n|---|---|---|---|\n";
    for (const auto& kv : ordered) rep << "| " << label(kv.first) << " | " << kv.first << " | " << kv.second.count << " of " << n << " | " << describe(m, kv.second.example) << " |\n";
    if (!found.empty()) {
      rep << "\nFull parameters of each smallest example, defaults included (the default of a parameter can be what triggers a finding, so check these too):\n\n";
      for (const auto& kv : ordered) rep << "- " << kv.first << ": " << describe(m, kv.second.example, true) << "\n";
      rep << "\nWhy, per finding:\n\n";
      for (const auto& kv : ordered) rep << "- " << kv.first << ": " << kv.second.detail << ". " << kv.second.why << "\n";
    }
    rep << "\nRe-run with the saved card: `RC_MODEL_CARD=" << out << "/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.\n"
        << "Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.\n";
    std::ofstream(out + "/report.md") << rep.str();
    {
      std::ostringstream h;
      h << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Scan of " << html_escape(name) << "</title><style>"
        << ":root{--bg:#fff;--fg:#1d2129;--mut:#5b6472;--card:#f5f6f8;--line:#2f6feb;--hot:#d92d20;--lim:#b54708;--bd:#d9dde3}"
        << "@media(prefers-color-scheme:dark){:root{--bg:#14171c;--fg:#e6e8ec;--mut:#9aa3b0;--card:#1d222a;--line:#6ea0ff;--hot:#ff6b5e;--lim:#f0a050;--bd:#2d3440}}"
        << "body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,sans-serif}main{max-width:980px;margin:0 auto;padding:24px 16px}"
        << "h1{font-size:22px;margin:0 0 4px}h2{font-size:16px;margin:0 0 6px}.mut{color:var(--mut)}.card{background:var(--card);border:1px solid var(--bd);border-radius:10px;padding:14px 16px;margin:16px 0}"
        << "code{font:13px ui-monospace,monospace;word-break:break-word}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:10px;margin-top:10px}"
        << "svg{width:100%;height:auto;background:var(--bg);border:1px solid var(--bd);border-radius:6px}.lab{font:11px ui-monospace,monospace;fill:var(--mut)}.lab.hot{fill:var(--hot)}"
        << ".ln{fill:none;stroke:var(--line);stroke-width:1.5}.ln.hot{stroke:var(--hot)}.lim{stroke:var(--lim);stroke-dasharray:4 3}.mark{stroke:var(--hot);stroke-dasharray:2 3;opacity:.7}"
        << "</style></head><body><main><h1>Scan of " << html_escape(name) << "</h1><p class=\"mut\">" << n << " scenarios (half pushed to parameter boundaries), " << pool.size() << " normal runs mined, " << inv.size() << " candidate invariants.</p>";
      if (found.empty()) h << "<div class=\"card\"><h2>No finding</h2><p class=\"mut\">This is not a certificate: a bug present in every run looks normal to learned invariants.</p></div>";
      for (const auto& kv : ordered) {
        h << "<section class=\"card\"><h2>" << label(kv.first) << ": " << html_escape(kv.first) << "</h2><p class=\"mut\">" << kv.second.count << " of " << n << " scenarios. " << html_escape(kv.second.detail) << ". " << html_escape(kv.second.why)
          << "</p><p><code>" << html_escape(describe(m, kv.second.example, true)) << "</code></p><div class=\"grid\">";
        for (const auto& sk : kv.second.sig) h << svg_plot(sk.first, sk.second, sk.first == kv.second.plot_signal, sk.first == kv.second.plot_signal && kv.second.has_limit, kv.second.limit, kv.second.at_step);
        h << "</div></section>";
      }
      h << "<p class=\"mut\">Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell. Dashed orange line: the learned limit. Red: the signal that broke it.</p></main></body></html>";
      std::ofstream(out + "/report.html") << h.str();
    }
    std::printf("\n%s\nreport: %s/report.md (and report.html)   card: %s/card.json\n", rep.str().c_str(), out.c_str(), out.c_str());
    return found.empty() ? 0 : 1;
  } catch (const std::exception& e) {
    std::cerr << "rc_scan: " << e.what() << "\n";
    return 2;
  }
}
