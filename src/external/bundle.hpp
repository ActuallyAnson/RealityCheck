// Regression-test bundle: scenario.json + expected.json + test.sh + README.md, runnable with rc_repro.
#pragma once
#include <sys/stat.h>

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#include "sim/scenario.hpp"
#include "util/json.hpp"

namespace rc {

inline void write_text(const std::string& path, const std::string& text) {
  std::ofstream f(path, std::ios::trunc);
  if (!f) throw std::runtime_error("cannot write " + path);
  f << text;
}

// `expected` must carry at least "domain" and "rule".
inline void write_bundle(const std::string& dir, const Scenario& sc, Json expected, const std::string& found_by) {
  if (std::system(("mkdir -p '" + dir + "'").c_str()) != 0) throw std::runtime_error("cannot create " + dir);
  const std::string domain = expected.str_or("domain", "rigid2d"), rule = expected.str_or("rule", "");
  Json sj = to_json(sc);
  sj["domain"] = domain;
  expected["found_by"] = found_by;
  write_text(dir + "/scenario.json", sj.dump() + "\n");
  write_text(dir + "/expected.json", expected.dump() + "\n");
  write_text(dir + "/test.sh",
             "#!/bin/sh\n"
             "# Regression test: exits 0 when the rule holds on ENGINE, 1 when the failure still reproduces, 2 on error.\n"
             "# usage: ./test.sh builtin | box2d | 'exec:./my_sim'      (RC_REPRO = path to the rc_repro binary)\n"
             "R=\"${RC_REPRO:-rc_repro}\"\n"
             "[ -x \"$R\" ] && R=\"$(cd \"$(dirname \"$R\")\" && pwd)/$(basename \"$R\")\"\n"
             "cd \"$(dirname \"$0\")\" && exec \"$R\" check --repro . --engine \"${1:?engine spec}\"\n");
  chmod((dir + "/test.sh").c_str(), 0755);
  write_text(dir + "/README.md",
             "# Regression test: " + rule + " (" + domain + ")\n\nA minimal scenario that makes `" + rule + "` fail. " + found_by +
                 "\n\n- `scenario.json`: the scenario, self-contained.\n- `expected.json`: the rule (and any explanation, which is a hypothesis).\n"
                 "- `./test.sh ENGINE`: runs it. Exit 1 = the failure reproduces, 0 = the rule holds.\n");
}

}  // namespace rc
