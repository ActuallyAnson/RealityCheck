#include "model/model.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>
#include <thread>

#include "util/rng.hpp"

namespace rc {

double cost_usd(const Usage& u, const Pricing& p) {
  long long fresh = std::max<long long>(0, u.prompt_tokens - u.cached_tokens);
  return (fresh * p.input + u.cached_tokens * p.cached_input + u.completion_tokens * p.output) / 1e6;
}

// ---------------------------------------------------------------- wire format

Json message_to_json(const ChatMessage& m) {
  Json j = Json::object();
  j["role"] = m.role;
  if (m.role == "tool") {
    j["tool_call_id"] = m.tool_call_id;
    j["content"] = m.content;
    return j;
  }
  if (m.role == "assistant") {
    if (m.content.empty() && !m.tool_calls.empty()) j["content"] = Json(); else j["content"] = m.content;
    if (!m.tool_calls.empty()) {
      Json calls = Json::array();
      for (const auto& c : m.tool_calls) {
        Json f = Json::object();
        f["name"] = c.name;
        f["arguments"] = c.arguments;
        Json o = Json::object();
        o["id"] = c.id;
        o["type"] = "function";
        o["function"] = f;
        calls.push(o);
      }
      j["tool_calls"] = calls;
    }
    return j;
  }
  if (m.images_b64.empty()) {
    j["content"] = m.content;
  } else {
    Json parts = Json::array();
    Json t = Json::object();
    t["type"] = "text";
    t["text"] = m.content;
    parts.push(t);
    for (const auto& b : m.images_b64) {
      Json url = Json::object();
      url["url"] = "data:image/png;base64," + b;
      Json p = Json::object();
      p["type"] = "image_url";
      p["image_url"] = url;
      parts.push(p);
    }
    j["content"] = parts;
  }
  return j;
}

Json build_chat_request(const ModelRequest& req) {
  Json j = Json::object();
  j["model"] = req.model;
  Json msgs = Json::array();
  for (const auto& m : req.messages) msgs.push(message_to_json(m));
  j["messages"] = msgs;
  if (req.tools.is_array() && !req.tools.arr().empty()) {
    Json tools = Json::array();
    for (const auto& t : req.tools.arr()) {
      Json w = Json::object();
      w["type"] = "function";
      w["function"] = t;  // {name, description, parameters}
      tools.push(w);
    }
    j["tools"] = tools;
    j["parallel_tool_calls"] = false;  // one call per turn keeps the transcript easy to cite
    // tool_choice is intentionally omitted: the API supports only "auto".
  }
  j["max_tokens"] = req.max_tokens;
  if (!req.reasoning_effort.empty()) j["reasoning_effort"] = req.reasoning_effort;
  return j;
}

ModelResponse parse_chat_response(const Json& body) {
  ModelResponse r;
  if (body.has("error")) {
    const Json& e = body.at("error");
    r.error = e.is_object() ? e.str_or("message", "API error") : (e.is_string() ? e.str() : "API error");
    return r;
  }
  if (!body.has("choices") || body.at("choices").arr().empty()) {
    r.error = "response has no choices";
    return r;
  }
  const Json& ch = body.at("choices").arr()[0];
  const Json& msg = ch.at("message");
  r.message.role = "assistant";
  if (msg.has("content") && msg.at("content").is_string()) r.message.content = msg.at("content").str();
  if (msg.has("tool_calls") && msg.at("tool_calls").is_array()) {
    for (const auto& c : msg.at("tool_calls").arr()) {
      ToolCall tc;
      tc.id = c.str_or("id", "");
      const Json& f = c.at("function");
      tc.name = f.str_or("name", "");
      if (f.has("arguments")) tc.arguments = f.at("arguments").is_string() ? f.at("arguments").str() : f.at("arguments").dump();
      r.message.tool_calls.push_back(tc);
    }
  }
  r.finish_reason = ch.str_or("finish_reason", "");
  if (body.has("usage")) {
    const Json& u = body.at("usage");
    r.usage.prompt_tokens = static_cast<long long>(u.num_or("prompt_tokens", 0));
    r.usage.completion_tokens = static_cast<long long>(u.num_or("completion_tokens", 0));
    // Field names for cached/reasoning tokens are not fully documented; accept the common spellings.
    if (u.has("prompt_tokens_details") && u.at("prompt_tokens_details").is_object())
      r.usage.cached_tokens = static_cast<long long>(u.at("prompt_tokens_details").num_or("cached_tokens", 0));
    else r.usage.cached_tokens = static_cast<long long>(u.num_or("cached_tokens", 0));
    if (u.has("completion_tokens_details") && u.at("completion_tokens_details").is_object())
      r.usage.reasoning_tokens = static_cast<long long>(u.at("completion_tokens_details").num_or("reasoning_tokens", 0));
  }
  r.ok = true;
  return r;
}

std::string request_key(const ModelRequest& req) {
  std::string d = build_chat_request(req).dump();
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(fnv1a(d.data(), d.size())));
  return b;
}

Json response_to_json(const ModelResponse& r) {
  Json j = Json::object();
  j["ok"] = r.ok;
  j["error"] = r.error;
  j["finish_reason"] = r.finish_reason;
  j["message"] = message_to_json(r.message);
  Json u = Json::object();
  u["prompt_tokens"] = static_cast<double>(r.usage.prompt_tokens);
  u["completion_tokens"] = static_cast<double>(r.usage.completion_tokens);
  u["cached_tokens"] = static_cast<double>(r.usage.cached_tokens);
  u["reasoning_tokens"] = static_cast<double>(r.usage.reasoning_tokens);
  j["usage"] = u;
  j["latency_ms"] = r.latency_ms;
  return j;
}

ModelResponse response_from_json(const Json& j) {
  ModelResponse r;
  r.ok = j.bool_or("ok", false);
  r.error = j.str_or("error", "");
  r.finish_reason = j.str_or("finish_reason", "");
  r.latency_ms = j.num_or("latency_ms", 0);
  const Json& m = j.at("message");
  r.message.role = "assistant";
  if (m.has("content") && m.at("content").is_string()) r.message.content = m.at("content").str();
  if (m.has("tool_calls"))
    for (const auto& c : m.at("tool_calls").arr()) {
      ToolCall tc;
      tc.id = c.at("id").str();
      tc.name = c.at("function").at("name").str();
      tc.arguments = c.at("function").at("arguments").str();
      r.message.tool_calls.push_back(tc);
    }
  const Json& u = j.at("usage");
  r.usage.prompt_tokens = static_cast<long long>(u.num_or("prompt_tokens", 0));
  r.usage.completion_tokens = static_cast<long long>(u.num_or("completion_tokens", 0));
  r.usage.cached_tokens = static_cast<long long>(u.num_or("cached_tokens", 0));
  r.usage.reasoning_tokens = static_cast<long long>(u.num_or("reasoning_tokens", 0));
  return r;
}

// ---------------------------------------------------------------- (a) real API client

ApiConfig ApiConfig::from_json(const Json& j) {
  ApiConfig c;
  c.base_url = j.str_or("base_url", c.base_url);
  c.model = j.str_or("model", c.model);
  c.api_key_env = j.str_or("api_key_env", c.api_key_env);
  c.native_tools = j.bool_or("native_tools", true);
  c.timeout_s = static_cast<int>(j.num_or("timeout_s", c.timeout_s));
  c.max_retries = static_cast<int>(j.num_or("max_retries", c.max_retries));
  if (j.has("pricing")) {
    c.pricing.input = j.at("pricing").num_or("input", c.pricing.input);
    c.pricing.cached_input = j.at("pricing").num_or("cached_input", c.pricing.cached_input);
    c.pricing.output = j.at("pricing").num_or("output", c.pricing.output);
  }
  return c;
}

namespace {

double now_ms() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Escape for a curl config file value inside double quotes.
std::string cfg_quote(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') o += '\\';
    if (c == '\n' || c == '\r') continue;
    o += c;
  }
  return o + "\"";
}

}  // namespace

ModelResponse MetaApiBackend::complete(const ModelRequest& req) {
  ModelResponse out;
  const char* key = std::getenv(cfg_.api_key_env.c_str());
  if (!key || !*key) {
    out.error = "environment variable " + cfg_.api_key_env + " is not set";
    return out;
  }
  ModelRequest r = req;
  if (r.model.empty()) r.model = cfg_.model;
  std::string body = build_chat_request(r).dump();

  // The key goes into a 0600 curl config file, never into argv (visible in `ps`) and never into logs.
  char tmpl[] = "/tmp/rc_curl_XXXXXX";
  int fd = mkstemp(tmpl);
  if (fd < 0) { out.error = "cannot create temp file"; return out; }
  std::string cfg_path = tmpl, body_path = cfg_path + ".json";
  fchmod(fd, 0600);
  close(fd);
  {
    std::ofstream c(cfg_path);
    c << "url = " << cfg_quote(cfg_.base_url + "/chat/completions") << "\n"
      << "header = " << cfg_quote(std::string("Authorization: Bearer ") + key) << "\n"
      << "header = \"Content-Type: application/json\"\n";
  }
  {
    std::ofstream b(body_path);
    b << body;
  }
  chmod(body_path.c_str(), 0600);

  std::mt19937 jitter(12345);
  for (int attempt = 0; attempt <= cfg_.max_retries; ++attempt) {
    std::string cmd = "curl -sS --max-time " + std::to_string(cfg_.timeout_s) + " -K '" + cfg_path +
                      "' --data-binary @'" + body_path + "' -w '\\n%{http_code}' 2>&1";
    double t0 = now_ms();
    FILE* p = popen(cmd.c_str(), "r");
    std::string text;
    if (p) {
      char buf[8192];
      size_t n;
      while ((n = fread(buf, 1, sizeof buf, p)) > 0) text.append(buf, n);
      pclose(p);
    }
    double lat = now_ms() - t0;
    size_t nl = text.find_last_of('\n');
    int status = nl == std::string::npos ? 0 : std::atoi(text.c_str() + nl + 1);
    std::string payload = nl == std::string::npos ? text : text.substr(0, nl);
    out = ModelResponse();
    out.http_status = status;
    out.latency_ms = lat;
    bool retryable = status == 429 || status >= 500 || status == 0;
    try {
      Json j = Json::parse(payload);
      ModelResponse parsed = parse_chat_response(j);
      parsed.http_status = status;
      parsed.latency_ms = lat;
      out = parsed;
      out.raw = payload.substr(0, 3000);
    } catch (const std::exception&) {
      out.raw = payload.substr(0, 3000);
      out.error = "non-JSON response (HTTP " + std::to_string(status) + "): " + payload.substr(0, 200);
    }
    if (out.ok || !retryable || attempt == cfg_.max_retries) break;
    int wait_ms = (500 << attempt) + static_cast<int>(jitter() % 250);  // exponential backoff with jitter
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
  }
  std::remove(cfg_path.c_str());
  std::remove(body_path.c_str());
  return out;
}

// ---------------------------------------------------------------- (b) replay / recording

ReplayBackend::ReplayBackend(const std::string& cache_path) {
  std::ifstream f(cache_path);
  if (!f) throw std::runtime_error("cannot read replay cache " + cache_path);
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    Json j = Json::parse(line);
    table_.push_back({j.at("key").str(), j.at("response")});
  }
  n_ = table_.size();
}

ModelResponse ReplayBackend::complete(const ModelRequest& req) {
  std::string k = request_key(req);
  for (const auto& e : table_)
    if (e.first == k) return response_from_json(e.second);
  ModelResponse r;
  r.error = "replay cache miss for request " + k + " (replay never makes a live call)";
  return r;
}

ModelResponse RecordingBackend::complete(const ModelRequest& req) {
  ModelResponse r = inner_.complete(req);
  if (r.ok) {
    Json line = Json::object();
    line["key"] = request_key(req);
    line["response"] = response_to_json(r);
    std::ofstream(path_, std::ios::app) << line.dump() << "\n";
  }
  return r;
}

// ---------------------------------------------------------------- (c) mock

ModelResponse MockBackend::text(const std::string& t, long long in_tokens, long long out_tokens) {
  ModelResponse r;
  r.ok = true;
  r.message.role = "assistant";
  r.message.content = t;
  r.finish_reason = "stop";
  r.usage.prompt_tokens = in_tokens;
  r.usage.completion_tokens = out_tokens;
  r.latency_ms = 1;
  return r;
}

ModelResponse MockBackend::tool(const std::string& name, const Json& args, const std::string& call_id,
                                long long in_tokens, long long out_tokens) {
  ModelResponse r = text("", in_tokens, out_tokens);
  r.message.tool_calls.push_back({call_id, name, args.dump()});
  r.finish_reason = "tool_calls";
  return r;
}

// ---------------------------------------------------------------- cost log + budget

BudgetedModel::BudgetedModel(ModelBackend& backend, Pricing pricing, double budget_usd, std::string log_path)
    : backend_(backend), pricing_(pricing), budget_(budget_usd), log_path_(std::move(log_path)) {}

std::string default_ledger_path() {
  if (const char* e = std::getenv("RC_LEDGER"); e && *e) return e;
  const char* home = std::getenv("HOME");
  return std::string(home && *home ? home : ".") + "/.realitycheck/spend_ledger.jsonl";
}

double default_lifetime_cap_usd() {
  if (const char* e = std::getenv("RC_LIFETIME_CAP_USD"); e && *e) return std::atof(e);
  return 100.0;
}

double ledger_total_usd(const std::string& path) {
  std::ifstream f(path);
  double total = 0;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    try { total += Json::parse(line).num_or("cost_usd", 0); } catch (const std::exception&) {}
  }
  return total;
}

void ledger_append(const std::string& path, const std::string& label, double cost_usd) {
  size_t slash = path.find_last_of('/');
  if (slash != std::string::npos) {
    std::string dir = path.substr(0, slash);
    mkdir(dir.c_str(), 0700);  // single level is enough for ~/.realitycheck
  }
  Json j = Json::object();
  j["label"] = label;
  j["cost_usd"] = cost_usd;
  j["time"] = static_cast<double>(std::time(nullptr));
  std::ofstream(path, std::ios::app) << j.dump() << "\n";
  chmod(path.c_str(), 0600);
}

ModelResponse BudgetedModel::complete(const ModelRequest& req, const std::string& label) {
  // Pessimistic pre-check: whole prompt uncached (about 3 characters per token, images at a flat allowance) plus the maximum output.
  // Image bytes are not text tokens: strip them and charge a flat allowance per image instead.
  ModelRequest text_only = req;
  size_t n_images = 0;
  for (ChatMessage& m : text_only.messages) { n_images += m.images_b64.size(); m.images_b64.clear(); }
  size_t chars = build_chat_request(text_only).dump().size();
  double in_tokens = static_cast<double>(chars) / 3.0 + 2000.0 * static_cast<double>(n_images);
  double worst = (in_tokens * pricing_.input + req.max_tokens * pricing_.output) / 1e6;
  if (spent_ + worst > budget_) {
    char b[200];
    std::snprintf(b, sizeof b, "budget $%.4f would be exceeded (spent $%.4f, next call up to $%.4f)", budget_, spent_, worst);
    throw BudgetExceeded(b);
  }
  if (!ledger_path_.empty()) {
    double total = ledger_total_usd(ledger_path_);
    if (total + worst > ledger_cap_) {
      char b[240];
      std::snprintf(b, sizeof b, "lifetime spend cap $%.2f would be exceeded (ledger total $%.4f, next call up to $%.4f); raise RC_LIFETIME_CAP_USD to continue",
                    ledger_cap_, total, worst);
      throw BudgetExceeded(b);
    }
  }
  ModelResponse r = backend_.complete(req);
  CostRecord rec;
  rec.call = static_cast<int>(log_.size()) + 1;
  rec.label = label;
  rec.usage = r.usage;
  rec.cost = r.ok ? cost_usd(r.usage, pricing_) : 0.0;
  rec.latency_ms = r.latency_ms;
  spent_ += rec.cost;
  if (!ledger_path_.empty() && rec.cost > 0) ledger_append(ledger_path_, label, rec.cost);
  rec.total_cost = spent_;
  log_.push_back(rec);
  if (!log_path_.empty()) {
    Json j = Json::object();
    j["call"] = rec.call;
    j["label"] = label;
    j["ok"] = r.ok;
    j["prompt_tokens"] = static_cast<double>(r.usage.prompt_tokens);
    j["cached_tokens"] = static_cast<double>(r.usage.cached_tokens);
    j["completion_tokens"] = static_cast<double>(r.usage.completion_tokens);
    j["reasoning_tokens"] = static_cast<double>(r.usage.reasoning_tokens);
    j["cost_usd"] = rec.cost;
    j["total_cost_usd"] = spent_;
    j["latency_ms"] = r.latency_ms;
    std::ofstream(log_path_, std::ios::app) << j.dump() << "\n";
  }
  return r;
}

Usage BudgetedModel::total_usage() const {
  Usage u;
  for (const auto& r : log_) {
    u.prompt_tokens += r.usage.prompt_tokens;
    u.completion_tokens += r.usage.completion_tokens;
    u.cached_tokens += r.usage.cached_tokens;
    u.reasoning_tokens += r.usage.reasoning_tokens;
  }
  return u;
}

double BudgetedModel::total_latency_ms() const {
  double t = 0;
  for (const auto& r : log_) t += r.latency_ms;
  return t;
}

}  // namespace rc
