#pragma once
#include "CodeGeneratorBase.h"

// Exports portable Python sources for the official lv_binding_micropython
// binding. The binding's pinned LVGL is deliberately independent of the editor.
class MicroPythonCodeGenerator final : public CodeGeneratorBase
{
public:
    bool ExportProject(std::string directory) override;
    bool Preflight(nlohmann::json& summary);
};
