#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <functional>
class PropertyWindow;
namespace InteractionEditor {
void Open(PropertyWindow* properties, std::function<nlohmann::json()> document);
void Close();
}
