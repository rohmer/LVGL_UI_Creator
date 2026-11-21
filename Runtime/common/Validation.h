#pragma once
#include "../../3rdParty/JSON/json.hpp"
namespace RuntimeValidation
{
void ValidateFonts(const nlohmann::json &document);
void ValidateFeatures(const nlohmann::json &document);
}
