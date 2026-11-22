#include "CodeGeneratorBase.h"
#include <Serialization/Protobuf.h>
#include <Serialization/RuntimeFeatures.h>
#include <Serialization/Xml.h>
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
        else if (path.extension() == ".xml" || path.extension() == ".XML")
        {
#if LVSERIAL_USE_XML
            const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            auto record = Serialization::Xml::Decode(bytes.data(), bytes.size());
            if (record.kind != Serialization::EntityKind::Project || record.type != "project") throw std::runtime_error("Expected an XML project record");
            document = std::move(record.data);
#else
            throw std::runtime_error("XML support is disabled in this build");
#endif
        }
        else document = nlohmann::json::parse(input);
        if (!LoadProject(std::move(document))) return false;
        sourceDirectory = path.parent_path().u8string(); return true;
    }
    catch (const std::exception& error) { lastError = error.what(); return false; }
}
