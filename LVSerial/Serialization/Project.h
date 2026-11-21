#pragma once
#include "Page.h"
#include "Screen.h"
#include "LVVariable.h"
namespace Serialization
{
    struct LoadedProject { std::vector<lv_obj_t*> objects; json variables; json screen; json metadata; json fonts = json::array(); json images = json::array(); json tries = json::array(); };
    class Project
    {
    public:
        static json ToJSON(const std::vector<lv_obj_t*>& objects, lv_obj_t* screen, const json& variables, int width = 0, int height = 0, const json& fonts = json::array(), const json& images = json::array(), const json& tries = json::array());
        static LoadedProject FromJSON(const json& document);
        static Binary ToProtobuf(const std::vector<lv_obj_t*>& objects, lv_obj_t* screen, const json& variables, int width = 0, int height = 0, const json& fonts = json::array(), const json& images = json::array(), const json& tries = json::array());
        static LoadedProject FromProtobuf(const Binary& input);
    };
}
