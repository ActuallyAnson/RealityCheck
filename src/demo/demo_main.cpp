// rc_demo: a small static file server for the web viewer (POSIX sockets, no third-party code).
//   rc_demo [--root web] [--port 8080] [--open]
// Serves files under --root only (requests containing ".." are refused). Listens on 127.0.0.1.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace {

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

void send_all(int fd, const std::string& s) {
  size_t off = 0;
  while (off < s.size()) {
    ssize_t n = send(fd, s.data() + off, s.size() - off, 0);
    if (n <= 0) return;
    off += static_cast<size_t>(n);
  }
}

void reply(int fd, int code, const std::string& type, const std::string& body) {
  const char* msg = code == 200 ? "OK" : code == 404 ? "Not Found" : code == 403 ? "Forbidden" : "Bad Request";
  std::ostringstream h;
  h << "HTTP/1.1 " << code << " " << msg << "\r\nContent-Type: " << type << "\r\nContent-Length: " << body.size()
    << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
  send_all(fd, h.str());
  send_all(fd, body);
}

}  // namespace

int main(int argc, char** argv) {
  std::string root = "web";
  int port = 8080;
  bool open_browser = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--root" && i + 1 < argc) root = argv[++i];
    else if (a == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
    else if (a == "--open") open_browser = true;
    else { std::cerr << "usage: rc_demo [--root DIR] [--port N] [--open]\n"; return 2; }
  }
  std::signal(SIGPIPE, SIG_IGN);
  int srv = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || listen(srv, 16) != 0) {
    std::cerr << "cannot listen on 127.0.0.1:" << port << " (is it already running?)\n";
    return 1;
  }
  std::cout << "RealityCheck viewer: http://127.0.0.1:" << port << "/  (serving " << root << ")\n" << std::flush;
  if (open_browser) {
    std::string cmd = "open 'http://127.0.0.1:" + std::to_string(port) + "/' >/dev/null 2>&1 || xdg-open 'http://127.0.0.1:" + std::to_string(port) + "/' >/dev/null 2>&1";
    if (std::system(cmd.c_str()) != 0) {}
  }
  for (;;) {
    int fd = accept(srv, nullptr, nullptr);
    if (fd < 0) continue;
    char buf[8192];
    ssize_t n = recv(fd, buf, sizeof buf - 1, 0);
    if (n <= 0) { close(fd); continue; }
    buf[n] = 0;
    std::string req(buf);
    std::istringstream ls(req);
    std::string method, target;
    ls >> method >> target;
    if (method != "GET") { reply(fd, 400, "text/plain", "GET only"); close(fd); continue; }
    size_t q = target.find('?');
    if (q != std::string::npos) target = target.substr(0, q);
    if (target.find("..") != std::string::npos || target.find('\\') != std::string::npos) {
      reply(fd, 403, "text/plain", "forbidden");
      close(fd);
      continue;
    }
    if (target == "/") target = "/index.html";
    std::string path = root + target;
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
      reply(fd, 404, "text/plain", "not found: " + target);
    } else {
      std::ifstream f(path, std::ios::binary);
      std::stringstream ss;
      ss << f.rdbuf();
      reply(fd, 200, mime(path), ss.str());
    }
    close(fd);
  }
}
