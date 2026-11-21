#pragma once
#include "ScreenValidation.h"
#include <cstdint>
namespace ProblemsPanel {
struct Context {
    std::function<nlohmann::json()> document;
    std::function<void(nlohmann::json)> apply;
    std::function<void(nlohmann::json)> applyMetadata;
    std::function<uint64_t()> revision;
    std::function<void(size_t)> selectScreen;
    std::function<void(size_t,int)> selectObject;
    std::function<size_t()> activeScreen;
};
lv_obj_t* Create(lv_obj_t* parent,PropertyWindow* properties,Context context);
void Close(lv_obj_t* panel=nullptr);
void Refresh(lv_obj_t* panel=nullptr);
size_t IssueCount(lv_obj_t* panel=nullptr);
uint64_t AnalysisCount(lv_obj_t* panel=nullptr);
}
