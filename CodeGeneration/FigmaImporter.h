#pragma once
#include "../3rdParty/JSON/json.hpp"
#include <cstddef>
#include <string>
#include <vector>

// Imports Figma REST JSON or the self-contained UI Creator Figma plugin bundle.
// Conversion does not touch LVGL objects or the editor's current document.
namespace FigmaImport {
using json = nlohmann::json;
struct Frame {
    std::string id, name;
    int width = 0, height = 0;
    bool hidden = false;
};
struct Options {
    std::vector<std::string> nodeIds;
    double scale = 1.0;
    bool includeHidden = false;
};
struct Result {
    json project;
    std::vector<std::string> warnings;
    size_t objects = 0;
};
std::vector<Frame> Frames(const json& source);
Result Convert(const json& source, const Options& options = {});
}
