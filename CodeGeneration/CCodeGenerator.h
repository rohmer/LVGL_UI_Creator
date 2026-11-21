#pragma once
#include "CodeGeneratorBase.h"

class CCodeGenerator final : public CodeGeneratorBase
{
public:
    void SetTargetVersion(std::string version) { targetVersion=std::move(version); }
    bool ExportProject(std::string directory) override;
    // Runs the complete C generation pipeline without writing output files.
    bool Preflight(nlohmann::json& summary);
    // Exact exported field/API suffix, including collisions in legacy names.
    static std::string VariableIdentifier(const nlohmann::json& records, size_t index);
private:
    std::string targetVersion="9.6";
};
