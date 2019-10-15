#pragma once
#include <vector>
#include "Variable/Variable.h"
#include "../3rdParty/JSON/json.hpp"

class ToolTray;

struct ObjectUserData
{
public:
    template <typename T>
    static std::vector<Variable<T>> variables;
    json objectJson;
    uint16_t objectID, parentID;
    ToolTray* toolTray;
};
