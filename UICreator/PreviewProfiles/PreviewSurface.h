#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include "ProfileModels.h"
#include "../../Runtime/include/lvgl_runtime.h"
#include <SDL3/SDL.h>
#include <memory>
#include <string>

namespace PreviewProfiles {
// An isolated native LVGL display. The image owns its pixels even after Freeze().
class PreviewSurface {
public:
    explicit PreviewSurface(lv_obj_t* parent);
    ~PreviewSurface();
    PreviewSurface(const PreviewSurface&)=delete;
    PreviewSurface& operator=(const PreviewSurface&)=delete;
    void Load(nlohmann::json document,int width,int height,const std::string& sourceDirectory={},bool interactive=false);
    void Freeze();
    void Clear();
    void Fit(int width,int height);
    lv_obj_t* Image() const;
    lvgl_runtime_t* Runtime() const;
    int Width() const;
    int Height() const;
    uint32_t Pixel(int x,int y) const;
    uint64_t PixelHash() const;
    bool HandleEvent(const SDL_Event& event);
    static PreviewSurface* Focused();
    static void Blur();
private:
    struct Impl;
    std::unique_ptr<Impl> state;
};
}
