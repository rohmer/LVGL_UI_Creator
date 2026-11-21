#pragma once
#include "LVObject.h"
#include "Protobuf.h"
namespace Serialization
{
    class Screen
    {
    public:
        static json ToJSON(lv_obj_t* screen, int width = 0, int height = 0);
        static json Normalize(const json& record);
        static lv_obj_t* FromJSON(const json& record, lv_obj_t* screen = nullptr);
        static Binary ToProtobuf(lv_obj_t* screen, int width = 0, int height = 0);
        static lv_obj_t* FromProtobuf(const Binary& record, lv_obj_t* screen = nullptr);
    };
}
