#pragma once

#include "LVObject.h"
#include <vector>

namespace Serialization
{
    class Page
    {
    public:
        static std::vector<lv_obj_t*> FromJSON(json document);
        static json ToJSON(std::vector<lv_obj_t*> objects);
    };
}
