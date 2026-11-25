#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <set>
#include <map>
#include <stdexcept>

namespace Serialization {
using json=nlohmann::json;
class ProjectFonts {
public:
    static json Normalize(const json& fonts) {
        if(!fonts.is_array())throw std::invalid_argument("Project fonts must be an array");
        std::set<std::string> ids,names;
        for(const auto& item:fonts) {
            const auto id=item.at("id").get<std::string>(),name=item.at("name").get<std::string>();
            if(id.empty()||name.empty()||id.find('\0')!=std::string::npos||name.find('\0')!=std::string::npos||name.find_first_of("\r\n")!=std::string::npos||!ids.insert(id).second||!names.insert(name).second)throw std::invalid_argument("Font IDs and names must be unique and nonempty");
            const auto& font=item.at("font");
            if(!font.is_object()||!font.contains("dsc")||!font["dsc"].contains("bitmap")||!font["dsc"]["bitmap"].is_string()||font.value("lineHeight",0)<=0)throw std::invalid_argument("Project font has no bitmap or valid metrics");
        }
        return fonts;
    }
    static void References(json& data,const json& fonts,bool expand) {
        std::map<std::string,json> byId;std::map<std::string,std::string> byData;
        for(const auto& font:Normalize(fonts)) {byId[font.at("id")]=font.at("font");byData[font.at("font").dump()]=font.at("id");}
        Walk(data,byId,byData,expand);
    }
    // Call on a materialized export copy. Catalog entries and authoring-only
    // libraries are not live roots; canvas inheritance and every screen are.
    static void PruneUnused(json& document) {
        const auto catalog=Normalize(document.value("fonts",json::array()));
        std::set<std::string> known,used;
        std::map<std::string,json> byId;
        std::map<std::string,std::string> byData;
        for(const auto& entry:catalog) {known.insert(entry.at("id"));byId[entry.at("id")]=entry.at("font");byData[entry.at("font").dump()]=entry.at("id");}
        auto require=[&](const json& value) {
            const auto id=value.get<std::string>();
            if(!known.count(id))throw std::invalid_argument("Unknown project font: "+id);
            used.insert(id);
        };
        auto visit=[&](const auto& self,const json& value)->void {
            if(value.is_array()){for(const auto& child:value)self(self,child);return;}
            if(!value.is_object())return;
            if(value.contains("fontId"))require(value.at("fontId"));
            if(value.size()==1&&value.contains("projectFont")){require(value.at("projectFont"));return;}
            for(const auto& field:value.items()) {
                if(field.key()=="design"||field.key()=="fonts"||field.key()=="components"||field.key()=="componentLibrary"||field.key()=="sharedStyles")continue;
                if((field.key()=="text_font"||field.key()=="font")&&field.value().is_object()) {
                    if(field.value().size()==1&&field.value().contains("projectFont"))require(field.value().at("projectFont"));
                    else {auto found=byData.find(field.value().dump());if(found!=byData.end())used.insert(found->second);}
                    continue;
                }
                self(self,field.value());
            }
        };
        auto screen=[&](json& value) {
            for(const auto* key:{"canvas","screen","obj"})if(value.contains(key)) {
                Walk(value.at(key),byId,byData,false);
                visit(visit,value.at(key));
            }
            if(!value.contains("canvas")&&!value.contains("screen")&&value.contains("base")) {
                Walk(value.at("base"),byId,byData,false);
                visit(visit,value.at("base"));
            }
        };
        if(document.contains("screens")&&document["screens"].is_array())for(auto& value:document["screens"])screen(value);
        else screen(document);
        if(document.contains("fonts")) {
            auto retained=json::array();for(const auto& entry:catalog)if(used.count(entry.at("id").get<std::string>()))retained.push_back(entry);
            document["fonts"]=std::move(retained);
        }
    }
private:
    static void Walk(json& data,const std::map<std::string,json>& ids,const std::map<std::string,std::string>& records,bool expand) {
        if(data.is_array()) {for(auto& item:data)Walk(item,ids,records,expand);return;}
        if(!data.is_object())return;
        if(data.contains("fontId")&&!ids.count(data.at("fontId").get<std::string>()))throw std::invalid_argument("Unknown widget font ID");
        if(data.contains("fontId")) {
            const auto id=data.at("fontId").get<std::string>();
            // The selected project font is authoritative for the main default
            // style. Preserve every other part/state selector, including its
            // independent font. This also repairs older compressed-font styles
            // whose generic snapshot could not carry a bitmap byte count.
            auto& styles=data["nativeStyles"];
            if(styles.is_null())styles=json::array();
            if(!styles.is_array())throw std::invalid_argument("Font styles must be an array");
            json* main=nullptr;
            for(auto& style:styles)if(style.value("selector",0u)==0u){main=&style;break;}
            if(!main){styles.push_back({{"selector",0},{"values",json::object()}});main=&styles.back();}
            (*main)["values"]["text_font"]=expand?ids.at(id):json{{"projectFont",id}};
        }
        if(data.size()==1&&data.contains("projectFont")) {
            const auto id=data.at("projectFont").get<std::string>();auto found=ids.find(id);if(found==ids.end())throw std::invalid_argument("Unknown project font: "+id);
            if(expand) data=found->second;
            return;
        }
        for(auto& item:data.items()) {
            if(item.key()=="fonts")continue;
            if(!expand&&(item.key()=="text_font"||item.key()=="font")&&item.value().is_object()) {
                auto found=records.find(item.value().dump());if(found!=records.end()){item.value()={{"projectFont",found->second}};continue;}
            }
            Walk(item.value(),ids,records,expand);
        }
    }
};
}
