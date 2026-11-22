#include "Project.h"
#include "ProjectFonts.h"
#include "ProjectImages.h"
#include "ProjectTries.h"
#include "AdvancedKeyboardMigration.h"
#include "ProjectDesign.h"
#include "ProjectScreens.h"
#include "Xml.h"
#include "RuntimeFeatures.h"
#include <stdexcept>
namespace Serialization
{
    json Project::ToJSON(const std::vector<lv_obj_t*>& objects, lv_obj_t* screen, const json& variables, int width, int height, const json& fonts, const json& images, const json& tries)
    {
        auto document = Page::ToJSON(objects);
        document["variables"] = LVVariable::NormalizeList(variables);
        document["canvas"] = Screen::ToJSON(screen, width, height);
        if(!fonts.empty()) {document["fonts"]=ProjectFonts::Normalize(fonts);ProjectFonts::References(document,fonts,false);}
        if(!images.empty()){document["images"]=ProjectImages::Normalize(images);ProjectImages::References(document,images,false);}
        if(!tries.is_array())throw std::invalid_argument("Project tries must be an array");
        // Authoring stores normalize on import/load. Keep frequent history and
        // dirty snapshots free of binary decoding and whole-model validation.
        if(!tries.empty())document["tries"]=tries;
        return document;
    }
    LoadedProject Project::FromJSON(const json& document)
    {
        if (!document.is_object() || (!document.contains("obj") && !document.contains("screens"))) throw std::invalid_argument("Invalid project document");
        if (document.value("formatVersion", 1) > 2) throw std::invalid_argument("Unsupported project version");
        auto source=document;ProjectScreens::Normalize(source);ProjectScreens::DenseIds(source);ProjectDesign::Materialize(source);MigrateAdvancedKeyboardEditors(source);
        if(!source.contains("obj")||!source["obj"].is_array())throw std::invalid_argument("Invalid project widgets");
        LoadedProject result;
        result.fonts=ProjectFonts::Normalize(source.value("fonts",json::array()));
        result.images=ProjectImages::Normalize(source.value("images",json::array()));
        result.tries=ProjectTries::Normalize(source.value("tries",json::array()));
        auto expanded=source;ProjectImages::References(expanded,result.images,true);ProjectFonts::References(expanded,result.fonts,true);
        auto tries = ProjectTries::Load(expanded);
        ProjectTries::Scope trieScope(tries);
        result.variables = LVVariable::NormalizeList(source.value("variables", json::array()));
        result.screen = Screen::Normalize(expanded.value("canvas", json::object()));
        result.metadata = source;
        result.metadata.erase("tries");
        result.objects = Page::FromJSON(expanded);
        return result;
    }
    Binary Project::ToProtobuf(const std::vector<lv_obj_t*>& objects, lv_obj_t* screen, const json& variables, int width, int height, const json& fonts, const json& images, const json& tries)
    { return Protobuf::Encode(EntityKind::Project, "project", ToJSON(objects, screen, variables, width, height, fonts, images, tries)); }
    LoadedProject Project::FromProtobuf(const Binary& input)
    {
        const auto record = Protobuf::Decode(input);
        if (record.kind != EntityKind::Project || record.type != "project") throw std::invalid_argument("Expected a project record");
        return FromJSON(record.data);
    }
    std::string Project::ToXML(const std::vector<lv_obj_t*>& objects, lv_obj_t* screen, const json& variables, int width, int height, const json& fonts, const json& images, const json& tries)
    {
#if LVSERIAL_USE_XML
        return Xml::Encode(EntityKind::Project, "project", ToJSON(objects, screen, variables, width, height, fonts, images, tries));
#else
        throw std::runtime_error("XML support is disabled in this build");
#endif
    }
    LoadedProject Project::FromXML(const std::string& input)
    {
#if LVSERIAL_USE_XML
        auto record = Xml::Decode(input.data(), input.size());
        if (record.kind != EntityKind::Project || record.type != "project") throw std::invalid_argument("Expected a UI Creator XML project");
        return FromJSON(record.data);
#else
        throw std::runtime_error("XML support is disabled in this build");
#endif
    }
}
