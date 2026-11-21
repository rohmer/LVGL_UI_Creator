#pragma once
#include "ScreenValidation.h"
namespace ScreenValidation {
std::vector<Issue> AccessibilityIssues(const nlohmann::json& document,const std::vector<lv_obj_t*>& live={});
double ContrastRatio(unsigned foreground,unsigned background);
std::string FixCaption(const nlohmann::json& document,const Issue& issue);
bool ApplyFix(nlohmann::json& document,const Issue& issue);
}
