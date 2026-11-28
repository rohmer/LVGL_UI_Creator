#pragma once

#include "lvgl/lvgl.h"
#include "../Utils/EditorEvents.h"
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../../3rdParty/JSON/json.hpp"

#include "../Utils/ObjectTools.h"
#if LVGL_VERSION_MAJOR >= 9
class ScreenFontMemory;
struct RuntimeKeyboardMemory;
#endif

class SimWindow
{
public:
    SimWindow(int iWidth, int iHeight);
    ~SimWindow();
    lv_obj_t* GetWindow() const { return window; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    void SetSize(int newWidth, int newHeight) { resize(newWidth, newHeight, false, false); }
    void SetKeyboardGroup(lv_group_t* group) { keyboardGroup = group; }
    void RefreshMemoryUsage();
    // Null means no global configuration; an object (even empty) overrides
    // legacy authored keyboards, matching the runtime materializer.
    std::function<nlohmann::json()> runtimeKeyboardSettings;

    lv_obj_t* GetDrawSurface()
    {
        return drawSurface;
    }

private:
    int width, height;
    lv_obj_t *window, *settingsWindow = nullptr, *drawSurface;
    lv_group_t* keyboardGroup = nullptr;
    lv_obj_t* memoryLabel = nullptr;
    size_t memoryBytes = static_cast<size_t>(-1);
    bool memoryEstimateUnavailable = false;
#if LVGL_VERSION_MAJOR >= 9
    std::unique_ptr<ScreenFontMemory> fontMemory;
    std::unique_ptr<RuntimeKeyboardMemory> runtimeKeyboardMemory;
#endif

    int updateFrame();

    struct cbStruct
    {
        SimWindow* sw;
        lv_obj_t *width, *height, *kb, *setBtn, *pos, *sze;
    };

    void resize(int newWidth, int newHeight, bool repos, bool resize);

    void createObjects();
    static void settings(lv_obj_t* obj, EditorEventCode event);
    static void closeSettings(lv_obj_t* obj, EditorEventCode event);
    static void dimensionChanged(lv_obj_t* obj, EditorEventCode event);
    static void focusDimension(cbStruct* controls, lv_obj_t* field);
    static void submitClicked(lv_obj_t* obj, EditorEventCode event);
};
