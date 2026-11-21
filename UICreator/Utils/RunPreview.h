#pragma once
#include <memory>
#include <functional>
#include <string>
#include "../../3rdParty/JSON/json.hpp"
#include <SDL3/SDL.h>
#include "../../Runtime/include/lvgl_runtime.h"
#include "../Performance/Metrics.h"

// A second LVGL display renders the portable protobuf runtime into the editor.
// Authored widgets, resources and variables are never used by this runtime.
class RunPreview {
public:
    RunPreview();
    ~RunPreview();
    void Start(nlohmann::json document, const std::string& sourceDirectory, std::function<void()> close);
    void Stop();
    bool Running() const;
    bool HandleEvent(const SDL_Event& event);
    lvgl_runtime_t* Runtime() const;
    lv_obj_t* Image() const;
    Performance::Snapshot Metrics() const;
    void SetProfilingEnabled(bool enabled);
    Performance::Distribution BenchmarkPrediction(const std::string& prefix,unsigned iterations=100);
private:
    struct Impl;
    std::unique_ptr<Impl> state;
};
