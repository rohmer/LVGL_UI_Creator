#pragma once
#include "LVFont.h"
#include <set>
#include <map>
#include <stdexcept>

namespace Serialization {
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
private:
    static void Walk(json& data,const std::map<std::string,json>& ids,const std::map<std::string,std::string>& records,bool expand) {
        if(data.is_array()) {for(auto& item:data)Walk(item,ids,records,expand);return;}
        if(!data.is_object())return;
        if(data.contains("fontId")&&!ids.count(data.at("fontId").get<std::string>()))throw std::invalid_argument("Unknown widget font ID");
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
