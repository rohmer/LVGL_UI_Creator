#pragma once
#include "CodeGeneratorBase.h"

// LVGL UI Creator's own XML representation of the shared project data model.
class XmlCodeGenerator final : public CodeGeneratorBase
{
public:
    void SetTargetVersion(std::string version) { targetVersion = std::move(version); }
    bool ExportProject(std::string file) override;
    bool Preflight(nlohmann::json& summary);
private:
    std::string targetVersion = "9.6";
};
