#pragma once
#include "NativeFontData.h"
#include <string>
#include <utility>

namespace FontConversion::Native {
FontData Rasterize(const Options& options, const std::atomic<bool>* cancel);
nlohmann::json ProjectFont(const FontData& font, const Options& options);
std::string CSource(const FontData& font, const Options& options);
std::vector<std::pair<std::filesystem::path, std::vector<uint8_t>>> Dump(
    const FontData& font, const Options& options, const std::atomic<bool>* cancel);
}
