// Model abstraction: one interface, three backends (real Meta API client, replay cache, scripted mock),
// plus cost accounting and a hard budget. Nothing here makes a network call unless MetaApiBackend::complete
// is invoked, and the CLI only does that behind an explicit --live flag.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "util/json.hpp"

namespace rc {

struct ToolCall {
  std::string id, name, arguments;  // arguments is a JSON string, as in the Chat Completions wire format
};

struct ChatMessage {
  std::string role;     // system | user | assistant | tool
  std::string content;
  std::vector<ToolCall> tool_calls;      // assistant only
  std::string tool_call_id;              // tool only
  std::vector<std::string> images_b64;   // PNG bytes, base64 (user only)
};

struct ModelRequest {
  std::string model;
  std::vector<ChatMessage> messages;
  Json tools;                // array of {name, description, parameters}; null/empty = no tools
  int max_tokens = 4096;
  std::string reasoning_effort;  // "" = omit
};

struct Usage {
  long long prompt_tokens = 0, completion_tokens = 0, cached_tokens = 0, reasoning_tokens = 0;
};

struct ModelResponse {
  bool ok = false;
  std::string error;
  int http_status = 0;
  ChatMessage message;  // role = assistant
  std::string finish_reason;
  Usage usage;
  double latency_ms = 0;
  std::string raw;  // live backend only: response body, truncated (never contains the key)
};

// USD per 1M tokens.
struct Pricing {
  double input = 1.25, cached_input = 0.15, output = 4.25;
};
double cost_usd(const Usage& u, const Pricing& p);

class ModelBackend {
public:
  virtual ~ModelBackend() = default;
  virtual std::string name() const = 0;
  virtual ModelResponse complete(const ModelRequest& req) = 0;
};

// ---- wire format (pure functions, unit-tested without network) ----
Json message_to_json(const ChatMessage& m);
Json build_chat_request(const ModelRequest& req);
ModelResponse parse_chat_response(const Json& body);
std::string request_key(const ModelRequest& req);  // canonical hash used by the replay cache
Json response_to_json(const ModelResponse& r);
ModelResponse response_from_json(const Json& j);

// ---- (a) real Meta Model API / any OpenAI-compatible Chat Completions endpoint ----
struct ApiConfig {
  std::string base_url = "https://api.meta.ai/v1";
  std::string model = "muse-spark-1.3";
  std::string api_key_env = "MODEL_API_KEY";
  Pricing pricing;
  bool native_tools = true;  // false: the agent falls back to the JSON action protocol
  int timeout_s = 120;
  int max_retries = 3;
  static ApiConfig from_json(const Json& j);
};

class MetaApiBackend : public ModelBackend {
public:
  explicit MetaApiBackend(ApiConfig cfg) : cfg_(std::move(cfg)) {}
  std::string name() const override { return "meta_api"; }
  ModelResponse complete(const ModelRequest& req) override;  // shells out to curl; key read from the env var
  const ApiConfig& config() const { return cfg_; }
private:
  ApiConfig cfg_;
};

// ---- (b) replay: serves recorded responses from a JSONL cache; a miss is an error, never a live call ----
class ReplayBackend : public ModelBackend {
public:
  explicit ReplayBackend(const std::string& cache_path);
  std::string name() const override { return "replay"; }
  ModelResponse complete(const ModelRequest& req) override;
  size_t entries() const { return n_; }
private:
  std::vector<std::pair<std::string, Json>> table_;
  size_t n_ = 0;
};

// Wraps a backend and appends every successful exchange to a JSONL cache (used to record live runs).
class RecordingBackend : public ModelBackend {
public:
  RecordingBackend(ModelBackend& inner, std::string cache_path) : inner_(inner), path_(std::move(cache_path)) {}
  std::string name() const override { return "recording(" + inner_.name() + ")"; }
  ModelResponse complete(const ModelRequest& req) override;
private:
  ModelBackend& inner_;
  std::string path_;
};

// ---- (c) scripted mock for tests ----
class MockBackend : public ModelBackend {
public:
  using Script = std::function<ModelResponse(const ModelRequest&, int call_index)>;
  explicit MockBackend(Script s) : script_(std::move(s)) {}
  std::string name() const override { return "mock"; }
  ModelResponse complete(const ModelRequest& req) override { return script_(req, calls_++); }
  int calls() const { return calls_; }
  // Helpers to build scripted replies.
  static ModelResponse text(const std::string& t, long long in_tokens = 800, long long out_tokens = 120);
  static ModelResponse tool(const std::string& name, const Json& args, const std::string& call_id,
                            long long in_tokens = 800, long long out_tokens = 120);
private:
  Script script_;
  int calls_ = 0;
};

// ---- cost logging and the budget stop ----
struct CostRecord {
  int call = 0;
  std::string label;  // e.g. "hunt", "report", "vision"
  Usage usage;
  double cost = 0, latency_ms = 0, total_cost = 0;
};

class BudgetExceeded : public std::runtime_error {
public:
  explicit BudgetExceeded(const std::string& m) : std::runtime_error(m) {}
};

// Decorator: logs tokens/cost/latency per call to an optional JSONL file and refuses calls once the
// budget is spent. The check is made before each call using the spend so far plus a pessimistic
// estimate of the next call, so the budget is not overshot by more than one call's actual cost.
class BudgetedModel {
public:
  BudgetedModel(ModelBackend& backend, Pricing pricing, double budget_usd, std::string log_path = "");
  // Lifetime guard across every process and run: each call is added to a JSONL ledger outside the repo, and a call that
  // could push the ledger total past cap_usd is refused. Only live backends should set this.
  void set_lifetime_ledger(std::string path, double cap_usd) { ledger_path_ = std::move(path); ledger_cap_ = cap_usd; }
  // Throws BudgetExceeded instead of making the call when the budget is exhausted.
  ModelResponse complete(const ModelRequest& req, const std::string& label);
  double spent() const { return spent_; }
  double budget() const { return budget_; }
  int calls() const { return static_cast<int>(log_.size()); }
  const std::vector<CostRecord>& log() const { return log_; }
  Usage total_usage() const;
  double total_latency_ms() const;
private:
  ModelBackend& backend_;
  Pricing pricing_;
  double budget_;
  std::string log_path_;
  double spent_ = 0;
  std::vector<CostRecord> log_;
  std::string ledger_path_;
  double ledger_cap_ = 0;
};

// Ledger helpers. Default path: $RC_LEDGER or ~/.realitycheck/spend_ledger.jsonl. Default cap: $RC_LIFETIME_CAP_USD or 100.
std::string default_ledger_path();
double default_lifetime_cap_usd();
double ledger_total_usd(const std::string& path);  // sum of "cost_usd" over the ledger (0 if missing)
void ledger_append(const std::string& path, const std::string& label, double cost_usd);

}  // namespace rc
