#pragma once
#include "DesignContext.h"
#include <map>

namespace ProjectAuthoring {
inline json& StyleTarget(json& record,const std::string& property)
{
    if(property.rfind("keyboard.",0)==0)return record["advancedKeyboard"][property.substr(9)];
    const auto colon=property.find(':');const unsigned selector=colon==std::string::npos?0:static_cast<unsigned>(std::stoul(property.substr(0,colon)));
    return Serialization::ProjectDesign::Style(record,selector)[colon==std::string::npos?property:property.substr(colon+1)];
}
inline json& SelectedRecord(json& document,int oid)
{
    if(oid<0)return document["canvas"];
    for(auto& record:document.at("obj"))if(record.value("oid",-2)==oid)return record;
    throw std::invalid_argument("The selected widget no longer exists");
}
inline const json& SelectedRecord(const json& document,int oid)
{
    static const json empty=json::object();
    if(oid<0)return document.contains("canvas")?document.at("canvas"):empty;
    for(const auto& record:document.at("obj"))if(record.value("oid",-2)==oid)return record;
    throw std::invalid_argument("The selected widget no longer exists");
}
inline json StyleBadges(const json& document,int oid)
{
    auto record=SelectedRecord(document,oid);const auto theme=record.value("base",json::object()).value("design",json::object()).value("theme",json::object());
    json output=json::array();if(theme.empty())return output;
    auto prototype=record;Serialization::ProjectDesign::BindTheme(prototype,theme.value("preset",std::string("project")),oid<0);
    const auto bindings=theme.value("bindings",json::object());const auto last=theme.value("last",json::object());
    for(const auto& item:prototype["base"]["design"]["theme"]["bindings"].items()) {
        auto current=StyleTarget(record,item.key());bool inherited=bindings.contains(item.key());
        if(inherited&&last.contains(item.key())&&!current.is_null()) {
            const auto& previous=last[item.key()];const bool color=item.key().find("color")!=std::string::npos||item.key()=="keyboard.keyBackground"||item.key()=="keyboard.keyForeground";
            inherited=current==previous||(color&&current.is_number_integer()&&previous.is_number_integer()&&((current.get<uint32_t>()&0xffffffu)==(previous.get<uint32_t>()&0xffffffu)));
        }
        output.push_back({{"property",item.key()},{"role",item.value()},{"inherited",inherited},{"value",current}});
    }
    return output;
}
inline void ResetStyleOverride(json& document,int oid,const std::string& property)
{
    auto record=SelectedRecord(static_cast<const json&>(document),oid);auto& theme=record["base"]["design"]["theme"];
    if(!theme.is_object()||!theme.contains("preset"))throw std::invalid_argument("Apply a project theme before resetting a shared-token override");
    auto prototype=record;Serialization::ProjectDesign::BindTheme(prototype,theme.value("preset",std::string("project")),oid<0);
    const auto bindings=prototype["base"]["design"]["theme"]["bindings"];
    if(!bindings.contains(property))throw std::invalid_argument("This property does not use a shared design token");
    theme["bindings"][property]=bindings[property];if(!theme["last"].is_object())theme["last"]=json::object();theme["last"].erase(property);
    Serialization::ProjectDesign::ResolveTheme(record,document);
    SelectedRecord(document,oid)=std::move(record);
}
}
