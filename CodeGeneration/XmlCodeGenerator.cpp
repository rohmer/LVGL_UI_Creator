#include "XmlCodeGenerator.h"
#include "CCodeGenerator.h"
#include "ProjectAssets.h"
#include "Targets/Target.h"
#include <Serialization/RuntimeFeatures.h>
#include <Serialization/Xml.h>
#include <Serialization/ProjectDesign.h>
#include <Serialization/ProjectScreens.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
#if LVSERIAL_USE_XML
std::string Prepare(const nlohmann::json& project, const std::string& source, const std::string& target)
{
    CGeneration::ValidateTarget(target);
    auto document = Serialization::ProjectScreens::Device(project);
    Serialization::ProjectDesign::Materialize(document, true);
    CGeneration::EmbedLocalImages(document, std::filesystem::u8path(source));
    document["runtimeTarget"] = target;
    // Share the existing structural, resource and binding preflight. XML keeps
    // the complete typed project, so the runtime needs no widget translation.
    CCodeGenerator validator;
    validator.SetSourceDirectory(source);
    validator.SetTargetVersion(target);
    nlohmann::json validation;
    if (!validator.LoadProject(document) || !validator.Preflight(validation))
        throw std::invalid_argument(validator.GetLastError());
    return Serialization::Xml::Encode(Serialization::EntityKind::Project, "project", document);
}
#endif
}

bool XmlCodeGenerator::Preflight(nlohmann::json& summary)
{
    summary = nlohmann::json::object();
    try {
#if LVSERIAL_USE_XML
        const auto xml = Prepare(project, sourceDirectory, targetVersion);
        summary = {{"format", "uicreator-xml"}, {"formatVersion", 1}, {"target", targetVersion},
                   {"files", 1}, {"sourceBytes", xml.size()}};
        lastError.clear(); return true;
#else
        throw std::runtime_error("XML support is disabled in this build (LVSERIAL_USE_XML)");
#endif
    } catch (const std::exception& error) { lastError = error.what(); return false; }
}

bool XmlCodeGenerator::ExportProject(std::string file)
{
    std::filesystem::path temporary;
    try {
#if LVSERIAL_USE_XML
        if (file.empty()) throw std::invalid_argument("Choose an XML output file");
        const auto xml = Prepare(project, sourceDirectory, targetVersion);
        const auto path = std::filesystem::u8path(file);
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        temporary = path;
        temporary += ".uicreator-tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot open XML output file");
        output.write(xml.data(), static_cast<std::streamsize>(xml.size())); output.close();
        if (!output) throw std::runtime_error("Could not finish writing XML output");
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace XML output file");
#else
        std::filesystem::rename(temporary, path);
#endif
        lastError.clear(); return true;
#else
        throw std::runtime_error("XML support is disabled in this build (LVSERIAL_USE_XML)");
#endif
    } catch (const std::exception& error) {
        if (!temporary.empty()) { std::error_code ignored; std::filesystem::remove(temporary, ignored); }
        lastError = error.what(); return false;
    }
}
