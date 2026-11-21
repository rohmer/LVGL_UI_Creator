#pragma once
#include <filesystem>
#include <string>
#include <lvgl/lvgl.h>

class PropertyWindow;
namespace LanguageModelWindow {
void Open(PropertyWindow* properties);
void LoadTrie(PropertyWindow* properties);
// Imports the model and applies it when the selected widget is Advanced Keyboard.
std::string Import(PropertyWindow* properties, const std::filesystem::path& path);
// Document/application teardown waits for cancelled native work before releasing UI state.
void Close();
void Shutdown();
lv_obj_t* ActiveWindow();
bool Running();
}
