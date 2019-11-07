#pragma once

#include <json.hpp>

class CodeGeneratorBase
{
public:
    CodeGeneratorBase();

    bool LoadProject(nlohmann::json project);
    bool LoadProject(std::string file);

    virtual bool ExportProject(std::string directory) = 0;



};