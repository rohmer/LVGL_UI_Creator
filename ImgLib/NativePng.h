#pragma once
#include <cstdint>
#include <vector>

// Independent of LVGL initialization, its allocator, and the UI thread.
namespace NativePng {
std::vector<uint8_t> Encode(const uint8_t* rgba, uint32_t width, uint32_t height);
}
