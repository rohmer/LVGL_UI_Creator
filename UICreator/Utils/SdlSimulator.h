#pragma once

#include <lvgl/lvgl.h>
#include <functional>

#if LVGL_VERSION_MAJOR >= 9
class SdlSimulator
{
public:
    bool Initialize(int width, int height);
    bool ProcessEvents();
    void SetShortcutHandler(std::function<bool(int,bool,bool,bool)> handler);
    lv_indev_t* GetKeyboard() const { return keyboard; }

    SdlSimulator() = default;
    ~SdlSimulator();
    SdlSimulator(const SdlSimulator&) = delete;
    SdlSimulator& operator=(const SdlSimulator&) = delete;

private:
    lv_display_t* display = nullptr;
    lv_indev_t* keyboard = nullptr;
    bool initialized = false;
    std::function<bool(int,bool,bool,bool)> shortcutHandler;
};
#endif
