#include "external/exec_sim.hpp"

#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>

#include "sim/cartpole.hpp"
#include "sim/orbit.hpp"

namespace rc {
namespace {

class Child {
public:
  explicit Child(const std::string& command) {
    int to_child[2], from_child[2];
    if (pipe(to_child) != 0 || pipe(from_child) != 0) throw std::runtime_error("pipe failed");
    pid_ = fork();
    if (pid_ < 0) throw std::runtime_error("fork failed");
    if (pid_ == 0) {
      dup2(to_child[0], 0);
      dup2(from_child[1], 1);
      close(to_child[0]); close(to_child[1]); close(from_child[0]); close(from_child[1]);
      execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    in_ = fdopen(to_child[1], "w");
    out_fd_ = from_child[0];
    signal(SIGPIPE, SIG_IGN);
  }
  ~Child() {
    if (in_) fclose(in_);   // closing stdin tells the child to exit
    if (out_fd_ >= 0) close(out_fd_);
    if (pid_ > 0) { kill(pid_, SIGTERM); int st; waitpid(pid_, &st, 0); }
  }
  Child(const Child&) = delete;
  Child& operator=(const Child&) = delete;

  std::string ask(const std::string& line) {
    if (fputs(line.c_str(), in_) < 0 || fputc('\n', in_) < 0 || fflush(in_) != 0)
      throw std::runtime_error("external simulator closed its input (did it crash?)");
    // One reply line, with a deadline: a simulator that never returns (a hang, an infinite loop) is killed and reported as an error,
    // never as a finding. RC_EXEC_TIMEOUT_S sets the per-request limit (default 60 s).
    const char* env = std::getenv("RC_EXEC_TIMEOUT_S");
    const double limit_s = env ? std::atof(env) : 60.0;
    for (;;) {
      size_t nl = buf_.find('\n');
      if (nl != std::string::npos) { std::string reply = buf_.substr(0, nl); buf_.erase(0, nl + 1); return reply; }
      pollfd p{out_fd_, POLLIN, 0};
      int r = poll(&p, 1, static_cast<int>(limit_s * 1000));
      if (r == 0) {
        kill(pid_, SIGKILL);
        throw std::runtime_error("external simulator did not answer within " + std::to_string(static_cast<int>(limit_s)) + " s and was killed (a hang, not a finding)");
      }
      char tmp[65536];
      ssize_t n = r < 0 ? -1 : read(out_fd_, tmp, sizeof tmp);
      if (n <= 0) {
        if (!buf_.empty()) { std::string reply = buf_; buf_.clear(); return reply; }
        throw std::runtime_error("external simulator closed its output (did it crash?)");
      }
      buf_.append(tmp, static_cast<size_t>(n));
    }
  }

private:
  pid_t pid_ = -1;
  FILE* in_ = nullptr;
  int out_fd_ = -1;
  std::string buf_;
};

const Json& need(const Json& j, const char* key) {
  if (!j.has(key)) throw std::runtime_error(std::string("external simulator reply lacks \"") + key + "\"");
  return j.at(key);
}

double num(const Json& v) {
  if (v.is_null()) return std::nan("");  // JSON has no NaN: null stands for a non-finite value
  return v.num();
}

Trace to_trace(const Scenario& sc, const Json& reply) {
  if (!reply.bool_or("ok", false))
    throw std::runtime_error("external simulator error: " + reply.str_or("error", "unspecified"));
  const Json::Arr& frames = need(reply, "frames").arr();
  if (static_cast<int>(frames.size()) != sc.steps + 1)
    throw std::runtime_error("external simulator returned " + std::to_string(frames.size()) + " frames, expected " +
                             std::to_string(sc.steps + 1));
  const size_t nbody = sc.domain == "cartpole" ? 1 : sc.bodies.size();
  Trace tr;
  tr.orbit = sc.domain == "orbit";
  double work = 0;
  for (size_t k = 0; k < frames.size(); ++k) {
    const Json::Arr& f = frames[k].arr();
    if (f.size() != 4 * nbody) throw std::runtime_error("frame " + std::to_string(k) + " must hold 4 numbers per body");
    StepRecord r;
    for (size_t i = 0; i < nbody; ++i) r.body.push_back({num(f[4 * i]), num(f[4 * i + 1]), num(f[4 * i + 2]), num(f[4 * i + 3])});
    if (sc.domain == "orbit") {
      double ke, pe;
      orbit_energies(sc, r, &ke, &pe);
      r.energy = ke + pe;
      for (size_t i = 0; i < nbody; ++i) {
        const double m = sc.bodies[i].m;
        const BodyState& q = r.body[i];
        r.px += m * q.vx; r.py += m * q.vy; r.ang += m * (q.x * q.vy - q.y * q.vx);
      }
    } else if (sc.domain == "cartpole") {
      r.energy = cartpole_energy(sc.cp, r.body[0]);
      if (reply.has("force")) r.fmag = std::fabs(num(reply.at("force").arr().at(k)));
      if (reply.has("work")) r.work = num(reply.at("work").arr().at(k));
      else if (k > 0) {  // trapezoid of (applied + pushed force) * velocity, from the reported controller force
        double t = (static_cast<double>(k) - 0.5) * sc.dt, ext = 0;
        for (const auto& p : sc.cp.pushes) if (t >= p.t && t < p.t + p.len) ext += p.force;
        double u = reply.has("force") ? num(reply.at("force").arr().at(k)) : 0;
        r.work = work + (u + ext) * 0.5 * (tr.rec.back().body[0].vx + r.body[0].vx) * sc.dt;
      }
      work = r.work;
    } else {
      double e = 0;  // rigid2d: kinetic + potential energy of all bodies (no controller)
      for (size_t i = 0; i < nbody; ++i) e += 0.5 * sc.bodies[i].m * (r.body[i].vx * r.body[i].vx + r.body[i].vy * r.body[i].vy) + sc.bodies[i].m * sc.gravity * r.body[i].y;
      r.energy = e;
    }
    tr.rec.push_back(std::move(r));
  }
  return tr;
}

}  // namespace

SimFn exec_sim(const std::string& command, const std::string& domain) {
  auto child = std::make_shared<Child>(command);
  auto lock = std::make_shared<std::mutex>();
  return [child, lock, domain](const Scenario& sc) {
    std::lock_guard<std::mutex> g(*lock);
    Json req = Json::object();
    req["op"] = "simulate";
    Json scen = to_json(sc);
    if (!scen.has("domain")) scen["domain"] = domain;
    req["scenario"] = scen;
    return to_trace(sc, Json::parse(child->ask(req.dump())));
  };
}

}  // namespace rc
