#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <string>
namespace PreviewProfiles {
const nlohmann::json& Presets();
nlohmann::json Profiles(const nlohmann::json& document);
void SaveProfile(nlohmann::json& document,const std::string& name,int width,int height);
nlohmann::json AtResolution(nlohmann::json document,int width,int height);
}
