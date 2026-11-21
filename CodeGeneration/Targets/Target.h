#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <map>
#include <string>
namespace CGeneration
{
void ValidateTarget(const std::string &target);
void NormalizePixels(nlohmann::json &record, bool canvas);
void PrepareTarget(nlohmann::json &project, const std::string &target, bool runtime = false);
void AdaptTarget(std::map<std::string, std::string> &files, const std::string &target);
} // namespace CGeneration
