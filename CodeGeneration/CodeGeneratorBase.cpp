#include "CodeGeneratorBase.h"
#include <Serialization/Protobuf.h>
#include <fstream>
#include <filesystem>

bool CodeGeneratorBase::LoadProject(nlohmann::json document)
{
    if (!document.is_object()) { lastError = "Project must be an object"; return false; }
    project = std::move(document); lastError.clear(); return true;
}
bool CodeGeneratorBase::LoadProject(std::string file)
{
    try
    {
        const auto path = std::filesystem::u8path(file);
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open project file");
        nlohmann::json document;
        if (path.extension() == ".pb")
        {
            const Serialization::Binary bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            auto record = Serialization::Protobuf::Decode(bytes);
            if (record.kind != Serialization::EntityKind::Project) throw std::runtime_error("Expected a project record");
            document = std::move(record.data);
        }
        else document = nlohmann::json::parse(input);
        if (!LoadProject(std::move(document))) return false;
        sourceDirectory = path.parent_path().u8string(); return true;
    }
    catch (const std::exception& error) { lastError = error.what(); return false; }
}
