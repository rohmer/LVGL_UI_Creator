#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

namespace NativeApng {
struct Frame {
    uint32_t width = 0, height = 0, delayMs = 100;
    std::vector<uint8_t> pixels; // Straight, top-down RGBA8, fully composited.
};
// Detect animation chunks without decoding; malformed animation metadata is
// routed to Decode so it cannot silently become a static first frame.
bool IsAnimated(const std::vector<uint8_t>& bytes);
std::vector<Frame> Decode(const std::vector<uint8_t>& bytes,
                          const std::atomic<bool>* cancel = nullptr);
}
