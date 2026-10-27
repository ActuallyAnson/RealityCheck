// rc_demo: serves the web viewer and runs live agent sessions (POSIX sockets and threads, no third-party code).
//   rc_demo [--root web] [--port 8080] [--open] [--config configs/hosted.json] [--budget-usd 0.25]
// Static files are served from --root only (".." is refused). Live runs are started with POST /api/live/start, which
// spawns rc_agent --live (it needs the API key env var, and the lifetime spend ledger applies), and are streamed to the
// page as Server-Sent Events. The server listens on 127.0.0.1 only, and the start endpoint rejects cross-origin requests.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "model/model.hpp"
#include "sim/bugs.hpp"
#include "util/json.hpp"
#include "viewer/view_export.hpp"

using namespace rc;

namespace {

struct Options {
  std::string root = "web", config = "configs/hosted.json", agent_path = "build/rc_agent";
  int port = 8080;
  double budget_usd = 0.25;
} g_opt;

struct LiveRun {
  std::mutex mu;
  bool active = false, finished = false;
  pid_t pid = 0;
  std::string id, dir, bug;
  int exit_code = 0;
} g_live;

std::string mime(const std::string& path) {
  static const std::map<std::string, std::string> m = {
      {".html", "text/html; charset=utf-8"}, {".js", "text/javascript; charset=utf-8"}, {".css", "text/css; charset=utf-8"},
      {".json", "application/json"}, {".png", "image/png"}, {".svg", "image/svg+xml"}, {".ico", "image/x-icon"},
      {".jsonl", "text/plain; charset=utf-8"}, {".txt", "text/plain; charset=utf-8"}};
  size_t d = path.find_last_of('.');
  if (d != std::string::npos) {
    auto it = m.find(path.substr(d));
    if (it != m.end()) return it->second;
  }
  return "application/octet-stream";
}

bool send_all(int fd, const std::string& s) {
  size_t off = 0;
  while (off < s.size()) {
    ssize_t n = send(fd, s.data() + off, s.size() - off, 0);
    if (n <= 0) return false;
    off += static_cast<size_t>(n);
  }
  return true;
}

void reply(int fd, int code, const std::string& type, const std::string& body) {
  const char* msg = code == 200 ? "OK" : code == 404 ? "Not Found" : code == 403 ? "Forbidden" : code == 409 ? "Conflict" : code == 500 ? "Server Error" : "Bad Request";
  std::ostringstream h;
  h << "HTTP/1.1 " << code << " " << msg << "\r\nContent-Type: " << type << "\r\nContent-Length: " << body.size()
    << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
  send_all(fd, h.str());
  send_all(fd, body);
}

std::string slurp(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

std::string lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string header_value(const std::string& req, const std::string& name) {
  std::string l = lower(req), key = "\r\n" + lower(name) + ":";
  size_t p = l.find(key);
  if (p == std::string::npos) return "";
  size_t s = p + key.size(), e = req.find("\r\n", s);
  std::string v = req.substr(s, e == std::string::npos ? std::string::npos : e - s);
  while (!v.empty() && v.front() == ' ') v.erase(v.begin());
  return v;
}

std::string query_value(const std::string& target, const std::string& key) {
  size_t q = target.find('?');
  if (q == std::string::npos) return "";
  std::string qs = target.substr(q + 1), pat = key + "=";
  size_t p = 0;
  while (p < qs.size()) {
    size_t e = qs.find('&', p);
    std::string part = qs.substr(p, e == std::string::npos ? std::string::npos : e - p);
    if (part.compare(0, pat.size(), pat) == 0) return part.substr(pat.size());
    if (e == std::string::npos) break;
    p = e + 1;
  }
  return "";
}

bool safe_token(const std::string& s) {
  if (s.empty() || s.size() > 64) return false;
  for (char c : s) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) return false;
  return true;
}

// ---- live runs ----

BugMask mask_for(const std::string& bug) { return bug == "none" ? 0 : bug_bit(bug_index(bug)); }

Json status_json() {
  Json j = Json::object();
  ApiConfig api = ApiConfig::from_json(Json::parse(slurp(g_opt.config)));
  const char* kv = std::getenv(api.api_key_env.c_str());
  j["key_set"] = kv != nullptr && *kv != 0;
  j["key_env"] = api.api_key_env;
  j["model"] = api.model;
  j["ledger_usd"] = ledger_total_usd(default_ledger_path());
  j["ledger_cap_usd"] = default_lifetime_cap_usd();
  j["run_budget_usd"] = g_opt.budget_usd;
  std::lock_guard<std::mutex> lk(g_live.mu);
  j["running"] = g_live.active && !g_live.finished;
  j["run"] = g_live.id;
  return j;
}

void reaper(pid_t pid) {
  int st = 0;
  waitpid(pid, &st, 0);
  std::lock_guard<std::mutex> lk(g_live.mu);
  g_live.finished = true;
  g_live.exit_code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

// Returns an error message, or "" and fills id on success.
std::string start_live(const std::string& bug, std::string* id_out) {
  if (bug != "none" && bug_index(bug) < 0) return "unknown bug id";
  Json st = status_json();
  if (!st.at("key_set").boolean()) return "the API key environment variable " + st.at("key_env").str() + " is not set in the shell that started rc_demo";
  if (ledger_total_usd(default_ledger_path()) + g_opt.budget_usd > default_lifetime_cap_usd()) return "lifetime spend cap would be exceeded";
  std::lock_guard<std::mutex> lk(g_live.mu);
  if (g_live.active && !g_live.finished) return "a live run is already in progress";
  std::time_t now = std::time(nullptr);
  std::string id = "live_" + std::to_string(now);
  std::string dir = "runs/live/" + id;
  if (std::system(("mkdir -p '" + dir + "'").c_str()) != 0) return "cannot create run directory";
  pid_t pid = fork();
  if (pid < 0) return "fork failed";
  if (pid == 0) {
    std::string log = dir + "/stdout.log";
    if (!freopen(log.c_str(), "w", stdout)) _exit(126);
    if (!freopen(log.c_str(), "a", stderr)) _exit(126);
    std::string budget = std::to_string(g_opt.budget_usd), cache = dir + "/cache.jsonl";
    std::vector<std::string> args = {g_opt.agent_path, "--live", "--config", g_opt.config, "--bugs", bug == "none" ? "" : bug, "--budget-usd", budget,
                                     "--reasoning-effort", "low", "--max-turns", "25", "--out", dir, "--record", cache};
    if (bug == "none") { args.erase(args.begin() + 4, args.begin() + 6); }
    std::vector<char*> av;
    for (auto& a : args) av.push_back(const_cast<char*>(a.c_str()));
    av.push_back(nullptr);
    execv(av[0], av.data());
    _exit(127);
  }
  g_live.active = true; g_live.finished = false; g_live.pid = pid; g_live.id = id; g_live.dir = dir; g_live.bug = bug; g_live.exit_code = 0;
  std::thread(reaper, pid).detach();
  *id_out = id;
  return "";
}

void sse_send(int fd, const Json& j, bool* ok) {
  if (!*ok) return;
  *ok = send_all(fd, "data: " + j.dump() + "\n\n");
}

void stream_live(int fd, const std::string& id) {
  std::string dir, bug;
  {
    std::lock_guard<std::mutex> lk(g_live.mu);
    if (g_live.id != id) { reply(fd, 404, "text/plain", "unknown run"); return; }
    dir = g_live.dir; bug = g_live.bug;
  }
  send_all(fd, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n");
  bool ok = true;
  LiveFeed feed(mask_for(bug));
  size_t consumed = 0;
  double last_cost_size = -1;
  std::string partial;
  auto finished = [&] { std::lock_guard<std::mutex> lk(g_live.mu); return g_live.finished; };
  for (int idle = 0; ok;) {
    bool fin = finished();  // read the flag BEFORE the files so nothing written just before exit is missed
    std::string txt = slurp(dir + "/events.jsonl");
    bool got = false;
    if (txt.size() > consumed) {
      partial += txt.substr(consumed);
      consumed = txt.size();
      size_t nl;
      while ((nl = partial.find('\n')) != std::string::npos) {
        std::string line = partial.substr(0, nl);
        partial.erase(0, nl + 1);
        if (line.empty()) continue;
        try {
          for (const Json& m : feed.push(Json::parse(line))) sse_send(fd, m, &ok);
          got = true;
        } catch (const std::exception&) {}
      }
    }
    std::string ctxt = slurp(dir + "/cost.jsonl");
    if (static_cast<double>(ctxt.size()) != last_cost_size) {
      last_cost_size = static_cast<double>(ctxt.size());
      double usd = 0; int calls = 0;
      std::stringstream cs(ctxt);
      std::string l;
      while (std::getline(cs, l)) if (!l.empty()) { try { usd += Json::parse(l).num_or("cost_usd", 0); ++calls; } catch (const std::exception&) {} }
      Json c = Json::object(); c["kind"] = "cost"; c["usd"] = usd; c["calls"] = calls;
      sse_send(fd, c, &ok);
    }
    if (fin && !got) {
      std::string err;
      if (slurp(dir + "/report.json").empty()) err = slurp(dir + "/stdout.log");
      if (!err.empty() && txt.empty()) {
        Json e = Json::object(); e["kind"] = "error"; e["message"] = err.substr(0, 600);
        sse_send(fd, e, &ok);
      } else {
        Json f = Json::object(); f["kind"] = "final"; f["doc"] = build_run_view(dir, mask_for(bug));
        sse_send(fd, f, &ok);
      }
      Json end = Json::object(); end["kind"] = "end";
      sse_send(fd, end, &ok);
      break;
    }
    if (!got && ++idle % 25 == 0) ok = send_all(fd, ": keepalive\n\n");
    if (got) idle = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
  }
}

void handle(int fd) {
  std::string req;
  char buf[8192];
  size_t hend = std::string::npos;
  while (hend == std::string::npos) {
    ssize_t n = recv(fd, buf, sizeof buf, 0);
    if (n <= 0) { close(fd); return; }
    req.append(buf, static_cast<size_t>(n));
    hend = req.find("\r\n\r\n");
    if (req.size() > 65536) { close(fd); return; }
  }
  std::string head = req.substr(0, hend + 2), body = req.substr(hend + 4);
  size_t clen = static_cast<size_t>(std::atoi(header_value(head, "Content-Length").c_str()));
  while (body.size() < clen && body.size() < 8192) {
    ssize_t n = recv(fd, buf, sizeof buf, 0);
    if (n <= 0) break;
    body.append(buf, static_cast<size_t>(n));
  }
  std::istringstream ls(head);
  std::string method, target;
  ls >> method >> target;
  std::string path = target.substr(0, target.find('?'));
  if (path.find("..") != std::string::npos || path.find('\\') != std::string::npos) { reply(fd, 403, "text/plain", "forbidden"); close(fd); return; }

  if (path == "/api/status" && method == "GET") {
    reply(fd, 200, "application/json", status_json().dump());
  } else if (path == "/api/live/start" && method == "POST") {
    std::string origin = header_value(head, "Origin");
    bool origin_ok = origin.empty() || origin == "http://127.0.0.1:" + std::to_string(g_opt.port) || origin == "http://localhost:" + std::to_string(g_opt.port);
    if (!origin_ok || header_value(head, "X-RC") != "1") {
      reply(fd, 403, "application/json", "{\"error\":\"cross-origin or missing X-RC header\"}");
    } else {
      std::string bug = "b01";
      try { bug = Json::parse(body).str_or("bug", "b01"); } catch (const std::exception&) {}
      std::string id;
      std::string err = start_live(bug, &id);
      Json j = Json::object();
      if (err.empty()) { j["run"] = id; reply(fd, 200, "application/json", j.dump()); }
      else { j["error"] = err; reply(fd, 409, "application/json", j.dump()); }
    }
  } else if (path == "/api/live/stream" && method == "GET") {
    std::string id = query_value(target, "run");
    if (!safe_token(id)) reply(fd, 400, "text/plain", "bad run id"); else stream_live(fd, id);
  } else if (method == "GET") {
    if (path == "/") path = "/index.html";
    std::string file = g_opt.root + path;
    struct stat st;
    if (stat(file.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) reply(fd, 404, "text/plain", "not found: " + path);
    else reply(fd, 200, mime(file), slurp(file));
  } else {
    reply(fd, 400, "text/plain", "unsupported");
  }
  close(fd);
}

}  // namespace

int main(int argc, char** argv) {
  bool open_browser = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--root" && i + 1 < argc) g_opt.root = argv[++i];
    else if (a == "--port" && i + 1 < argc) g_opt.port = std::atoi(argv[++i]);
    else if (a == "--config" && i + 1 < argc) g_opt.config = argv[++i];
    else if (a == "--budget-usd" && i + 1 < argc) g_opt.budget_usd = std::atof(argv[++i]);
    else if (a == "--open") open_browser = true;
    else { std::cerr << "usage: rc_demo [--root DIR] [--port N] [--open] [--config FILE] [--budget-usd N]\n"; return 2; }
  }
  std::string self = argv[0];
  size_t slash = self.find_last_of('/');
  g_opt.agent_path = (slash == std::string::npos ? std::string(".") : self.substr(0, slash)) + "/rc_agent";
  signal(SIGPIPE, SIG_IGN);
  int srv = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(static_cast<uint16_t>(g_opt.port));
  if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || listen(srv, 32) != 0) {
    std::cerr << "cannot listen on 127.0.0.1:" << g_opt.port << " (is it already running?)\n";
    return 1;
  }
  std::cout << "RealityCheck viewer: http://127.0.0.1:" << g_opt.port << "/  (serving " << g_opt.root << ")\n";
  Json st = status_json();
  std::cout << "live runs: " << (st.at("key_set").boolean() ? "ready" : "DISABLED (" + st.at("key_env").str() + " is not set; recorded mode still works)")
            << ", per-run cap $" << g_opt.budget_usd << ", lifetime ledger $" << st.at("ledger_usd").num() << " of $" << st.at("ledger_cap_usd").num() << "\n" << std::flush;
  if (open_browser) {
    std::string cmd = "open 'http://127.0.0.1:" + std::to_string(g_opt.port) + "/' >/dev/null 2>&1 || xdg-open 'http://127.0.0.1:" + std::to_string(g_opt.port) + "/' >/dev/null 2>&1";
    if (std::system(cmd.c_str()) != 0) {}
  }
  for (;;) {
    int fd = accept(srv, nullptr, nullptr);
    if (fd < 0) continue;
    std::thread(handle, fd).detach();
  }
}
