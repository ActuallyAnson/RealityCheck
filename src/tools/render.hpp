// Software rasteriser + PNG writer for frame strips (no external image library).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "sim/scenario.hpp"
#include "sim/world.hpp"

namespace rc {

// Draws the given steps side by side (wrapping rows of up to `cols`). Body `highlight` is drawn red.
// Each tile is labelled with its step number. Returns PNG file bytes.
std::vector<uint8_t> render_strip_png(const Scenario& sc, const Trace& tr, const std::vector<int>& steps,
                                      int highlight = -1, int cols = 3, int px_per_unit = 18);

std::string base64_encode(const std::vector<uint8_t>& data);

}  // namespace rc
