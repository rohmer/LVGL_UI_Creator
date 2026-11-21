#pragma once
#include "../../3rdParty/JSON/json.hpp"
class PropertyWindow;
namespace ImageConverterWindow { void Open(PropertyWindow* properties, const nlohmann::json& source=nlohmann::json()); void Close(); }
