#pragma once
#include "../Utils/EditorEdit.h"
#include <Serialization/ProjectFonts.h>
#include <Serialization/ObjectUserData.h>
#include <map>

class FontLibrary {
    json records=json::array();
    std::map<std::string,const lv_font_t*> fonts;
public:
    const json& ToJSON() const {return records;}
    void Load(const json& data) { EditorEdit::Scope edit;
        auto normalized=Serialization::ProjectFonts::Normalize(data);std::map<std::string,const lv_font_t*> loaded;
        for(const auto& item:normalized)loaded[item.at("id")]=&Serialization::LVFont::FromJSON(item.at("font"));
        records=std::move(normalized);fonts=std::move(loaded);
    }
    const lv_font_t* Get(const std::string& id) const {auto found=fonts.find(id);if(found==fonts.end())throw std::invalid_argument("Project font not found");return found->second;}
    std::string Add(std::string name,const json& font,const json& settings=json::object()) {
        if(name.empty())name="Converted font";for(char& c:name)if(c=='\n'||c=='\r')c=' ';
        for(const auto& record:records)if(record.at("font")==font)return record.at("id");
        auto stem=name;size_t suffix=2;auto exists=[&](const std::string& n){for(auto& r:records)if(r.at("name")==n)return true;return false;};
        while(exists(name))name=stem+" ("+std::to_string(suffix++)+")";
        std::string id;size_t index=1;do{id="font_"+std::to_string(index++);}while(fonts.count(id));
        auto next=records;next.push_back({{"id",id},{"name",name},{"font",font},{"conversion",settings}});Serialization::ProjectFonts::Normalize(next);fonts[id]=&Serialization::LVFont::FromJSON(font);records=std::move(next);return id;
    }
    void Rename(const std::string& id,const std::string& name) { EditorEdit::Scope edit;auto next=records;bool found=false;for(auto& record:next)if(record.at("id")==id){record["name"]=name;found=true;}if(!found)throw std::invalid_argument("Font not found");Serialization::ProjectFonts::Normalize(next);records=std::move(next);}
    void Remove(const std::string& id) { EditorEdit::Scope edit;auto next=json::array();for(auto& record:records)if(record.at("id")!=id)next.push_back(record);records=std::move(next);fonts.erase(id);}
    static std::string Selection(lv_obj_t* object) {auto* data=static_cast<ObjectUserData*>(lv_obj_get_user_data(object));if(!data)return {};auto base=data->objectJson.find("base");return base!=data->objectJson.end()?base->value("fontId",std::string{}):std::string{};}
    void Apply(lv_obj_t* object,const std::string& id) { EditorEdit::Scope edit;
        if(!object)return;
        if(id.empty())lv_obj_remove_local_style_prop(object,LV_STYLE_TEXT_FONT,LV_PART_MAIN);
        else lv_obj_set_style_text_font(object,Get(id),LV_PART_MAIN);
        auto* data=static_cast<ObjectUserData*>(lv_obj_get_user_data(object));if(!data){Serialization::LVObject::FromJSON(json::object(),object);data=static_cast<ObjectUserData*>(lv_obj_get_user_data(object));}
        if(id.empty())data->objectJson["base"].erase("fontId");else data->objectJson["base"]["fontId"]=id;
    }
    size_t Usage(lv_obj_t* root,const std::string& id) const {size_t count=Selection(root)==id?1:0;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)count+=Usage(lv_obj_get_child(root,i),id);return count;}
    void Rebind(lv_obj_t* root) {auto id=Selection(root);if(!id.empty())Apply(root,id);for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)Rebind(lv_obj_get_child(root,i));}
};
