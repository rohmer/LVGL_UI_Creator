#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <lvgl/lvgl.h>
#include <functional>
#include <string>
#include <vector>
class PropertyWindow;
namespace ScreenValidation {
enum class Severity { Error, Warning };
struct Issue {
    Severity severity;
    std::string group;
    size_t screen=0;
    int oid=-1;
    std::string message;
    std::string variableName;
    int bindingIndex=-1;
};
// The optional objects are the current screen's authored objects in saved order.
// Live geometry/fonts allow checking flex layouts and the actual fallback fonts.
std::vector<Issue> Analyze(const nlohmann::json& document,const std::vector<lv_obj_t*>& objects={});
std::string BlockingErrors(const nlohmann::json& document,const std::vector<lv_obj_t*>& objects={});
void Open(PropertyWindow* properties,std::function<nlohmann::json()> document);
void Close();
}
