#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <filesystem>
namespace CGeneration
{
void EmbedLocalImages(nlohmann::json &project, const std::filesystem::path &source);
}
