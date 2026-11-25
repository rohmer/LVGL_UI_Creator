#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "../../3rdParty/JSON/json.hpp"

namespace FontConversion {
struct Source {
    std::filesystem::path path;
    std::string ranges = "0x20-0x7E";
    std::string symbols;
    int hinting = 0; // Light, off, strong (per source).
};
struct Options {
    std::vector<Source> sources;
    std::filesystem::path output;
    std::string format = "lvgl", name = "ui_font_24", include = "lvgl.h", fallback;
    std::filesystem::path relativeRoot;
    int size = 24, bpp = 4, smoothing = 0, stride = 0, align = 1;
    bool compress = true, prefilter = true, kerning = true, fastKerning = false, colorInfo = false, fullInfo = false;
};
struct Result {
    bool success = false;
    std::string message;
    std::vector<uint8_t> preview;
    // Self-contained project resource, retaining the selected bit depth,
    // compression and layout. Preview bytes are only for the editor display.
    nlohmann::json font;
    std::vector<std::filesystem::path> files;
    size_t bytes = 0;
};
// Validate options and describe the equivalent converter options for diagnostics.
std::vector<std::string> Arguments(const Options& options, bool preview = false);
// Rasterize into a project asset without requiring or creating output files.
Result Prepare(const Options& options, const std::atomic<bool>* cancel = nullptr);
Result Convert(const Options& options, const std::atomic<bool>* cancel = nullptr);
std::string SymbolName(const std::filesystem::path& source, int size);
}
