#pragma once
#include <vector>
#include "../../3rdParty/JSON/json.hpp"

class ToolTray;
using json = nlohmann::json;

struct ObjectUserData
{
public:
    json objectJson;
    uint16_t objectID, parentID;
    ToolTray* toolTray;
    std::string objectName;
};
