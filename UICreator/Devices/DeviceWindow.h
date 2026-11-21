#pragma once
#include <lvgl/lvgl.h>
#include "../../3rdParty/JSON/json.hpp"
#include <filesystem>
#include <functional>
class PropertyWindow;
namespace DeviceWindow {
struct Context {
    std::function<nlohmann::json()> document;
    std::function<void(nlohmann::json)> apply;
    std::function<void(nlohmann::json)> applyMetadata;
};
void SetContext(Context context);
void Open(PropertyWindow* properties);
void OpenProject(PropertyWindow* properties,const std::filesystem::path& project,const nlohmann::json& manifest={});
void Close();
void Shutdown();
lv_obj_t* ActiveWindow();
bool Running();
}
