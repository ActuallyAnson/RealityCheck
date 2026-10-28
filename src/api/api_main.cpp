// rc_api_server: the deterministic tools as a local HTTP API, or as an MCP server on stdin/stdout.
//   rc_api_server --port 8090 [--token T] [--allow-exec]      HTTP on 127.0.0.1 only; POST /v1/{run,shrink,bundle,hunt,random_scenario}, GET /v1/domains
//   rc_api_server --mcp [--allow-exec]                         Model Context Protocol over stdio (newline-delimited JSON-RPC 2.0)
// No model and no API key are involved. exec: engines (arbitrary programs) are refused unless --allow-exec is given.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include "api/api.hpp"

using namespace rc;

namespace {
ApiOptions g_opt;
std::string g_token;

std::string lower(std::string s) { for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }

std::string header_value(const std::string& head, const std::string& name) {
  std::string l = lower(head), key = "\r\n" + lower(name) + ":";
  size_t p = l.find(key);
  if (p == std::string::npos) return "";
  size_t s = p + key.size(), e = head.find("\r\n", s);
  std::string v = head.substr(s, e == std::string::npos ? std::string::npos : e - s);
  while (!v.empty() && v.front() == ' ') v.erase(v.begin());
  return v;
}

void reply(int fd, int code, const std::string& type, const std::string& body) {
  const char* msg = code == 200 ? "OK" : code == 401 ? "Unauthorized" : code == 404 ? "Not Found" : code == 413 ? "Payload Too Large" : "Bad Request";
  std::ostringstream h;
  h << "HTTP/1.1 " << code << " " << msg << "\r\nContent-Type: " << type << "\r\nContent-Length: " << body.size() << "\r\nConnection: close\r\n\r\n";
  std::string all = h.str() + body;
  size_t off = 0;
  while (off < all.size()) { ssize_t n = send(fd, all.data() + off, all.size() - off, 0); if (n <= 0) break; off += static_cast<size_t>(n); }
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
    if (req.size() > 65536 && hend == std::string::npos) { close(fd); return; }
  }
  std::string head = req.substr(0, hend + 2), body = req.substr(hend + 4);
  size_t clen = static_cast<size_t>(std::atol(header_value(head, "Content-Length").c_str()));
  if (clen > (1u << 20)) { reply(fd, 413, "application/json", "{\"ok\":false,\"error\":\"body over 1 MB\"}"); close(fd); return; }
  while (body.size() < clen) { ssize_t n = recv(fd, buf, sizeof buf, 0); if (n <= 0) break; body.append(buf, static_cast<size_t>(n)); }
  std::istringstream ls(head);
  std::string method, path;
  ls >> method >> path;
  if (!g_token.empty() && header_value(head, "Authorization") != "Bearer " + g_token) {
    reply(fd, 401, "application/json", "{\"ok\":false,\"error\":\"missing or wrong bearer token\"}");
  } else if (method == "GET" && path == "/v1/domains") {
    reply(fd, 200, "application/json", api_call("domains", Json::object(), g_opt).dump());
  } else if (method == "GET" && path == "/v1/openapi.yaml") {
    std::ifstream f("docs/openapi.yaml");
    std::stringstream ss; ss << f.rdbuf();
    reply(fd, f ? 200 : 404, "text/yaml", ss.str());
  } else if (method == "POST" && path.compare(0, 4, "/v1/") == 0) {
    Json args;
    try { args = Json::parse(body.empty() ? "{}" : body); } catch (const std::exception&) { reply(fd, 400, "application/json", "{\"ok\":false,\"error\":\"body is not valid JSON\"}"); close(fd); return; }
    Json r = api_call(path.substr(4), args, g_opt);
    reply(fd, r.bool_or("ok", false) ? 200 : 400, "application/json", r.dump());
  } else {
    reply(fd, 404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
  }
  close(fd);
}

// ---- MCP over stdio ----
Json rpc_result(const Json& id, Json result) { Json r = Json::object(); r["jsonrpc"] = "2.0"; r["id"] = id; r["result"] = std::move(result); return r; }
Json rpc_error(const Json& id, int code, const std::string& m) {
  Json r = Json::object(); r["jsonrpc"] = "2.0"; r["id"] = id;
  Json e = Json::object(); e["code"] = code; e["message"] = m; r["error"] = e;
  return r;
}

int run_mcp() {
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
    Json req;
    try { req = Json::parse(line); } catch (const std::exception&) { std::cout << rpc_error(Json(), -32700, "parse error").dump() << "\n" << std::flush; continue; }
    std::string method = req.str_or("method", "");
    bool has_id = req.has("id");
    Json id = has_id ? req.at("id") : Json();
    if (!has_id) continue;  // notifications (initialized, cancelled, ...) need no answer
    Json res;
    if (method == "initialize") {
      Json r = Json::object();
      r["protocolVersion"] = req.has("params") ? req.at("params").str_or("protocolVersion", "2024-11-05") : "2024-11-05";
      Json caps = Json::object(); caps["tools"] = Json::object();
      r["capabilities"] = caps;
      Json info = Json::object(); info["name"] = "realitycheck"; info["version"] = "1.0";
      r["serverInfo"] = info;
      res = rpc_result(id, r);
    } else if (method == "ping") {
      res = rpc_result(id, Json::object());
    } else if (method == "tools/list") {
      Json r = Json::object(); r["tools"] = mcp_tool_list();
      res = rpc_result(id, r);
    } else if (method == "tools/call") {
      std::string name = req.has("params") ? req.at("params").str_or("name", "") : "";
      Json args = req.has("params") && req.at("params").has("arguments") ? req.at("params").at("arguments") : Json::object();
      if (name.compare(0, 3, "rc_") != 0) { res = rpc_error(id, -32602, "unknown tool"); }
      else {
        Json out = api_call(name.substr(3), args, g_opt);
        Json r = Json::object();
        Json c = Json::object(); c["type"] = "text"; c["text"] = out.dump();
        Json arr = Json::array(); arr.push(c);
        r["content"] = arr;
        r["isError"] = !out.bool_or("ok", false);
        res = rpc_result(id, r);
      }
    } else res = rpc_error(id, -32601, "method not found");
    std::cout << res.dump() << "\n" << std::flush;
  }
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  bool mcp = false;
  int port = 8090;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--mcp") mcp = true;
    else if (a == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
    else if (a == "--token" && i + 1 < argc) g_token = argv[++i];
    else if (a == "--allow-exec") g_opt.allow_exec = true;
    else { std::cerr << "usage: rc_api_server [--port N] [--token T] [--allow-exec] | --mcp\n"; return 2; }
  }
  if (mcp) return run_mcp();
  signal(SIGPIPE, SIG_IGN);
  int srv = socket(AF_INET, SOCK_STREAM, 0), one = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  sockaddr_in addr{};
  addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); addr.sin_port = htons(static_cast<uint16_t>(port));
  if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || listen(srv, 32) != 0) { std::cerr << "cannot listen on 127.0.0.1:" << port << "\n"; return 1; }
  std::cerr << "RealityCheck API on http://127.0.0.1:" << port << "/v1/  (no model, no key; exec engines " << (g_opt.allow_exec ? "ENABLED" : "refused") << ")\n";
  for (;;) { int fd = accept(srv, nullptr, nullptr); if (fd >= 0) std::thread(handle, fd).detach(); }
}
