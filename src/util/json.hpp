// Minimal JSON value, parser and serializer. Objects are ordered maps so output is canonical.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace rc {

class Json {
public:
  enum Type { Null, Bool, Number, String, Array, Object };
  using Arr = std::vector<Json>;
  using Obj = std::map<std::string, Json>;

  Json() = default;
  Json(std::nullptr_t) {}
  Json(bool b) : t_(Bool), b_(b) {}
  Json(int n) : t_(Number), n_(n) {}
  Json(long n) : t_(Number), n_(static_cast<double>(n)) {}
  Json(long long n) : t_(Number), n_(static_cast<double>(n)) {}
  Json(unsigned long n) : t_(Number), n_(static_cast<double>(n)) {}
  Json(double n) : t_(Number), n_(n) {}
  Json(const char* s) : t_(String), s_(s) {}
  Json(std::string s) : t_(String), s_(std::move(s)) {}
  Json(Arr a) : t_(Array), a_(std::move(a)) {}
  Json(Obj o) : t_(Object), o_(std::move(o)) {}

  static Json array() { return Json(Arr{}); }
  static Json object() { return Json(Obj{}); }

  Type type() const { return t_; }
  bool is_null() const { return t_ == Null; }
  bool is_object() const { return t_ == Object; }
  bool is_array() const { return t_ == Array; }
  bool is_number() const { return t_ == Number; }
  bool is_string() const { return t_ == String; }
  bool is_bool() const { return t_ == Bool; }

  double num() const { need(Number); return n_; }
  bool boolean() const { need(Bool); return b_; }
  const std::string& str() const { need(String); return s_; }
  const Arr& arr() const { need(Array); return a_; }
  Arr& arr() { need(Array); return a_; }
  const Obj& obj() const { need(Object); return o_; }

  bool has(const std::string& k) const { return t_ == Object && o_.count(k) > 0; }
  const Json& at(const std::string& k) const {
    need(Object);
    auto it = o_.find(k);
    if (it == o_.end()) throw std::runtime_error("missing key: " + k);
    return it->second;
  }
  Json& operator[](const std::string& k) {
    if (t_ == Null) { t_ = Object; }
    need(Object);
    return o_[k];
  }
  void push(Json v) {
    if (t_ == Null) { t_ = Array; }
    need(Array);
    a_.push_back(std::move(v));
  }
  double num_or(const std::string& k, double d) const { return has(k) && at(k).is_number() ? at(k).num() : d; }
  bool bool_or(const std::string& k, bool d) const { return has(k) && at(k).is_bool() ? at(k).boolean() : d; }
  std::string str_or(const std::string& k, const std::string& d) const {
    return has(k) && at(k).is_string() ? at(k).str() : d;
  }

  std::string dump() const { std::string out; write(out); return out; }
  static Json parse(const std::string& text) {
    size_t i = 0;
    Json v = parse_value(text, i);
    skip(text, i);
    if (i != text.size()) throw std::runtime_error("trailing characters in JSON");
    return v;
  }

private:
  Type t_ = Null;
  bool b_ = false;
  double n_ = 0;
  std::string s_;
  Arr a_;
  Obj o_;

  void need(Type t) const {
    if (t_ != t) throw std::runtime_error("JSON type mismatch");
  }

  static void esc(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
      switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
          if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
          else out += static_cast<char>(c);
      }
    }
    out += '"';
  }

  void write(std::string& out) const {
    switch (t_) {
      case Null: out += "null"; break;
      case Bool: out += b_ ? "true" : "false"; break;
      case Number: {
        if (!std::isfinite(n_)) { out += "null"; break; }  // JSON has no NaN/Inf
        char b[40];
        if (n_ == std::floor(n_) && std::fabs(n_) < 1e15) std::snprintf(b, sizeof b, "%.0f", n_);
        else std::snprintf(b, sizeof b, "%.17g", n_);
        out += b;
        break;
      }
      case String: esc(out, s_); break;
      case Array: {
        out += '[';
        for (size_t i = 0; i < a_.size(); ++i) { if (i) out += ','; a_[i].write(out); }
        out += ']';
        break;
      }
      case Object: {
        out += '{';
        bool first = true;
        for (const auto& kv : o_) {
          if (!first) out += ',';
          first = false;
          esc(out, kv.first);
          out += ':';
          kv.second.write(out);
        }
        out += '}';
        break;
      }
    }
  }

  static void skip(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r')) ++i;
  }

  static Json parse_value(const std::string& s, size_t& i) {
    skip(s, i);
    if (i >= s.size()) throw std::runtime_error("unexpected end of JSON");
    char c = s[i];
    if (c == '{') {
      ++i;
      Json o = Json::object();
      skip(s, i);
      if (i < s.size() && s[i] == '}') { ++i; return o; }
      for (;;) {
        skip(s, i);
        Json k = parse_value(s, i);
        if (!k.is_string()) throw std::runtime_error("object key must be string");
        skip(s, i);
        if (i >= s.size() || s[i] != ':') throw std::runtime_error("expected ':'");
        ++i;
        o.o_[k.s_] = parse_value(s, i);
        skip(s, i);
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == '}') { ++i; return o; }
        throw std::runtime_error("expected ',' or '}'");
      }
    }
    if (c == '[') {
      ++i;
      Json a = Json::array();
      skip(s, i);
      if (i < s.size() && s[i] == ']') { ++i; return a; }
      for (;;) {
        a.a_.push_back(parse_value(s, i));
        skip(s, i);
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == ']') { ++i; return a; }
        throw std::runtime_error("expected ',' or ']'");
      }
    }
    if (c == '"') {
      ++i;
      std::string r;
      while (i < s.size() && s[i] != '"') {
        if (s[i] == '\\') {
          ++i;
          if (i >= s.size()) break;
          switch (s[i]) {
            case 'n': r += '\n'; break;
            case 't': r += '\t'; break;
            case 'r': r += '\r'; break;
            case 'b': r += '\b'; break;
            case 'f': r += '\f'; break;
            case 'u': {
              if (i + 4 >= s.size()) throw std::runtime_error("bad \\u escape");
              unsigned cp = static_cast<unsigned>(std::strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16));
              if (cp < 0x80) r += static_cast<char>(cp);
              else if (cp < 0x800) { r += static_cast<char>(0xC0 | (cp >> 6)); r += static_cast<char>(0x80 | (cp & 0x3F)); }
              else { r += static_cast<char>(0xE0 | (cp >> 12)); r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); r += static_cast<char>(0x80 | (cp & 0x3F)); }
              i += 4;
              break;
            }
            default: r += s[i];
          }
          ++i;
        } else r += s[i++];
      }
      if (i >= s.size()) throw std::runtime_error("unterminated string");
      ++i;
      return Json(r);
    }
    if (s.compare(i, 4, "true") == 0) { i += 4; return Json(true); }
    if (s.compare(i, 5, "false") == 0) { i += 5; return Json(false); }
    if (s.compare(i, 4, "null") == 0) { i += 4; return Json(); }
    char* end = nullptr;
    double d = std::strtod(s.c_str() + i, &end);
    if (end == s.c_str() + i) throw std::runtime_error("invalid JSON value");
    i = static_cast<size_t>(end - s.c_str());
    return Json(d);
  }
};

}  // namespace rc
