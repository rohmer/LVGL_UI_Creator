#pragma once
#include <Serialization/ProjectDesign.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/AdvancedKeyboard.h>
#include <functional>
#include <cstdint>
#include <string>
#include <vector>

namespace ProjectAuthoring {
using json=nlohmann::json;
struct DesignContext {
    std::function<json()> document;
    std::function<void(json)> apply;
    std::function<std::vector<int>()> selection;
    std::function<void(std::vector<int>)> select;
    lv_group_t* keyboardGroup=nullptr;
    std::string sourceDirectory;
    std::function<void(json)> applyMetadata;
    std::function<uint64_t()> revision;
};
inline void ApplyNewDefaults(lv_obj_t* object,const json& document)
{
    if(!object||!document.contains("projectTheme"))return;
    auto record=Serialization::ObjectSerializer::SerializeObject(object);
    Serialization::ProjectDesign::ApplyDefaults(document,record);
    Serialization::LVObject::FromJSON(record.at("base"),object);
    if(record.contains("advancedKeyboard"))Serialization::AdvancedKeyboard::Apply(object,record.at("advancedKeyboard"));
}
}
