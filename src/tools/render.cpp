#include "tools/render.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace rc {
namespace {

struct Img {
  int w, h;
  std::vector<uint8_t> px;  // RGB
  Img(int w_, int h_) : w(w_), h(h_), px(static_cast<size_t>(w_) * h_ * 3, 255) {}
  void set(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    size_t i = (static_cast<size_t>(y) * w + x) * 3;
    px[i] = r; px[i + 1] = g; px[i + 2] = b;
  }
};

struct Col { uint8_t r, g, b; };

void fill_rect(Img& im, int x0, int y0, int x1, int y1, Col c) {
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) im.set(x, y, c.r, c.g, c.b);
}

void fill_circle(Img& im, double cx, double cy, double r, Col c) {
  int x0 = static_cast<int>(std::floor(cx - r)), x1 = static_cast<int>(std::ceil(cx + r));
  int y0 = static_cast<int>(std::floor(cy - r)), y1 = static_cast<int>(std::ceil(cy + r));
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
      if (dx * dx + dy * dy <= r * r) im.set(x, y, c.r, c.g, c.b);
    }
}

// 3x5 digit font for tile labels.
const char* kDigits[10] = {"111101101101111", "010110010010111", "111001111100111", "111001111001111",
                           "101101111001001", "111100111001111", "111100111101111", "111001001001001",
                           "111101111101111", "111101111001111"};

void draw_number(Img& im, int x, int y, int n, int scale, Col c) {
  std::string s = std::to_string(n);
  for (char ch : s) {
    const char* g = kDigits[ch - '0'];
    for (int row = 0; row < 5; ++row)
      for (int col = 0; col < 3; ++col)
        if (g[row * 3 + col] == '1') fill_rect(im, x + col * scale, y + row * scale, x + (col + 1) * scale, y + (row + 1) * scale, c);
    x += 4 * scale;
  }
}

uint32_t crc32(const uint8_t* d, size_t n, uint32_t crc = 0) {
  static uint32_t table[256];
  static bool init = false;
  if (!init) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      table[i] = c;
    }
    init = true;
  }
  crc = ~crc;
  for (size_t i = 0; i < n; ++i) crc = table[(crc ^ d[i]) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

void put32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(static_cast<uint8_t>(x >> 24)); v.push_back(static_cast<uint8_t>(x >> 16));
  v.push_back(static_cast<uint8_t>(x >> 8)); v.push_back(static_cast<uint8_t>(x));
}

void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
  put32(out, static_cast<uint32_t>(data.size()));
  std::vector<uint8_t> body(type, type + 4);
  body.insert(body.end(), data.begin(), data.end());
  out.insert(out.end(), body.begin(), body.end());
  put32(out, crc32(body.data(), body.size()));
}

std::vector<uint8_t> encode_png(const Img& im) {
  std::vector<uint8_t> raw;
  for (int y = 0; y < im.h; ++y) {
    raw.push_back(0);  // filter: none
    raw.insert(raw.end(), im.px.begin() + static_cast<long>(y) * im.w * 3, im.px.begin() + static_cast<long>(y + 1) * im.w * 3);
  }
  std::vector<uint8_t> z = {0x78, 0x01};  // zlib header, stored (uncompressed) deflate blocks
  size_t pos = 0;
  do {
    size_t n = std::min<size_t>(65535, raw.size() - pos);
    bool last = pos + n >= raw.size();
    z.push_back(last ? 1 : 0);
    z.push_back(static_cast<uint8_t>(n & 0xFF)); z.push_back(static_cast<uint8_t>(n >> 8));
    z.push_back(static_cast<uint8_t>(~n & 0xFF)); z.push_back(static_cast<uint8_t>((~n >> 8) & 0xFF));
    z.insert(z.end(), raw.begin() + static_cast<long>(pos), raw.begin() + static_cast<long>(pos + n));
    pos += n;
  } while (pos < raw.size());
  uint32_t a = 1, b = 0;
  for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
  put32(z, (b << 16) | a);
  std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  std::vector<uint8_t> ihdr;
  put32(ihdr, static_cast<uint32_t>(im.w)); put32(ihdr, static_cast<uint32_t>(im.h));
  ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
  chunk(out, "IHDR", ihdr);
  chunk(out, "IDAT", z);
  chunk(out, "IEND", {});
  return out;
}

}  // namespace

std::vector<uint8_t> render_strip_png(const Scenario& sc, const Trace& tr, const std::vector<int>& steps, int highlight,
                                      int cols, int ppu) {
  const int tw = static_cast<int>(sc.arena_w * ppu), th = static_cast<int>(sc.arena_h * ppu), gap = 4;
  int n = std::max<int>(1, static_cast<int>(steps.size()));
  cols = std::max(1, std::min(cols, n));
  int rows = (n + cols - 1) / cols;
  Img im(cols * tw + (cols + 1) * gap, rows * th + (rows + 1) * gap);
  fill_rect(im, 0, 0, im.w, im.h, {40, 40, 48});
  for (size_t t = 0; t < steps.size(); ++t) {
    int k = std::max(0, std::min(steps[t], tr.steps()));
    int ox = gap + static_cast<int>(t % cols) * (tw + gap), oy = gap + static_cast<int>(t / cols) * (th + gap);
    fill_rect(im, ox, oy, ox + tw, oy + th, {250, 250, 245});
    for (const auto& w : sc.walls)
      fill_rect(im, ox + static_cast<int>(w.x * ppu), oy + static_cast<int>(th - (w.y + w.h) * ppu),
                ox + static_cast<int>((w.x + w.w) * ppu + 0.999), oy + static_cast<int>(th - w.y * ppu + 0.999), {60, 60, 70});
    if (sc.ctrl.enabled)
      for (const auto& wp : sc.ctrl.waypoints) {
        int cx = ox + static_cast<int>(wp.x * ppu), cy = oy + static_cast<int>(th - wp.y * ppu);
        fill_rect(im, cx - 4, cy - 1, cx + 5, cy + 2, {40, 160, 70});
        fill_rect(im, cx - 1, cy - 4, cx + 2, cy + 5, {40, 160, 70});
      }
    const StepRecord& rec = tr.rec[static_cast<size_t>(k)];
    for (size_t i = 0; i < rec.body.size(); ++i) {
      const BodyState& q = rec.body[i];
      if (!std::isfinite(q.x) || !std::isfinite(q.y)) continue;
      Col c = {70, 120, 200};
      if (sc.ctrl.enabled && static_cast<int>(i) == sc.ctrl.body) c = {230, 150, 30};
      if (static_cast<int>(i) == highlight) c = {215, 40, 40};
      fill_circle(im, ox + q.x * ppu, oy + th - q.y * ppu, sc.bodies[i].r * ppu, c);
    }
    fill_rect(im, ox, oy, ox + 40, oy + 12, {255, 255, 255});
    draw_number(im, ox + 3, oy + 3, k, 2, {0, 0, 0});
  }
  return encode_png(im);
}

std::string base64_encode(const std::vector<uint8_t>& d) {
  static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  size_t i = 0;
  for (; i + 2 < d.size(); i += 3) {
    uint32_t v = (d[i] << 16) | (d[i + 1] << 8) | d[i + 2];
    out += T[v >> 18]; out += T[(v >> 12) & 63]; out += T[(v >> 6) & 63]; out += T[v & 63];
  }
  if (i + 1 == d.size()) {
    uint32_t v = d[i] << 16;
    out += T[v >> 18]; out += T[(v >> 12) & 63]; out += "==";
  } else if (i + 2 == d.size()) {
    uint32_t v = (d[i] << 16) | (d[i + 1] << 8);
    out += T[v >> 18]; out += T[(v >> 12) & 63]; out += T[(v >> 6) & 63]; out += '=';
  }
  return out;
}

}  // namespace rc
