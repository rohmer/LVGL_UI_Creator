#pragma once
#include "../3rdParty/JSON/json.hpp"
#include <string>

class CodeGeneratorBase
{
public:
    virtual ~CodeGeneratorBase() = default;
    bool LoadProject(nlohmann::json project);
    bool LoadProject(std::string file);
    virtual bool ExportProject(std::string directory) = 0;
    const std::string& GetLastError() const { return lastError; }
    void SetSourceDirectory(std::string directory) { sourceDirectory = std::move(directory); }
protected:
    nlohmann::json project;
    std::string lastError;
    std::string sourceDirectory;
};
