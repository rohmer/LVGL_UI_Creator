#pragma once
#include <lvgl/lvgl.h>
#include <string>
class PropertyWindow;
namespace FontConverterWindow {
void Open(PropertyWindow* properties,const std::string& fontId={});
void OpenBuiltin(PropertyWindow* properties,const std::string& builtinId);
void Close();
}
