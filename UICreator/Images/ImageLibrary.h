#pragma once
#include "../Utils/EditorEdit.h"
#include <Serialization/ProjectImages.h>
#include <Serialization/ObjectUserData.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/ExtendedWidgets.h>
class ImageLibrary {
    json records=json::array();
public:
    const json& ToJSON() const {return records;}
    void Load(const json& data){ EditorEdit::Scope edit;records=Serialization::ProjectImages::Normalize(data);}
    const json& Get(const std::string& id) const {for(const auto& r:records)if(r.at("id")==id)return r.at("image");throw std::invalid_argument("Project image not found");}
    std::string Add(std::string name,const json& input){ EditorEdit::Scope edit;
        auto image=input;auto decoded=Serialization::ImageAsset::Decode(image);image["width"]=decoded.width;image["height"]=decoded.height;image["frames"]=decoded.delays.size();
        for(const auto& r:records)if(r.at("image")==image)return r.at("id");
        if(name.empty())name="Image";for(char& c:name)if(c=='\r'||c=='\n')c=' ';
        auto stem=name;size_t n=2;auto exists=[&](const std::string& key,const std::string& value){for(const auto& r:records)if(r.at(key)==value)return true;return false;};
        while(exists("name",name))name=stem+" ("+std::to_string(n++)+")";
        n=1;std::string id;do{id="image_"+std::to_string(n++);}while(exists("id",id));
        auto next=records;next.push_back({{"id",id},{"name",name},{"image",image}});Load(next);return id;
    }
    void Rename(const std::string& id,const std::string& name){ EditorEdit::Scope edit;auto next=records;bool found=false;for(auto& r:next)if(r.at("id")==id){r["name"]=name;found=true;}if(!found)throw std::invalid_argument("Select an image first");Load(next);}
    void Remove(const std::string& id){ EditorEdit::Scope edit;auto next=json::array();for(const auto& r:records)if(r.at("id")!=id)next.push_back(r);records=std::move(next);}
    static std::string Selection(lv_obj_t* object){auto* d=static_cast<ObjectUserData*>(lv_obj_get_user_data(object));return d&&d->objectJson.contains("base")?d->objectJson["base"].value("imageId",std::string{}):std::string{};}
    void Apply(lv_obj_t* object,const std::string& id){ EditorEdit::Scope edit;
        if(!object||lv_obj_get_class(object)!=&lv_image_class)throw std::invalid_argument("Select an image widget first");
        if(id.empty()){throw std::invalid_argument("Select a project image");}else Serialization::ImageAsset::Restore(object,Get(id));
        auto* d=static_cast<ObjectUserData*>(lv_obj_get_user_data(object));if(!d){Serialization::LVObject::FromJSON(json::object(),object);d=static_cast<ObjectUserData*>(lv_obj_get_user_data(object));}
        if(id.empty())d->objectJson["base"].erase("imageId");else d->objectJson["base"]["imageId"]=id;
    }
    static std::string ButtonSelection(lv_obj_t* object,size_t state,size_t side){
        const auto record=Serialization::ObjectSerializer::SerializeObject(object);const auto& source=record.at("imagebutton").at("states").at(state).at(side);
        return source.is_object()?source.value("imageId",std::string{}):std::string{};
    }
    void ApplyButton(lv_obj_t* object,size_t state,size_t side,const std::string& id,bool keepCurrent=false){ EditorEdit::Scope edit;
        if(!object||lv_obj_get_class(object)!=&lv_imagebutton_class||state>=LV_IMAGEBUTTON_STATE_NUM||side>=3)throw std::invalid_argument("Invalid image button state");
        auto record=Serialization::ObjectSerializer::SerializeObject(object);auto& source=record["imagebutton"]["states"].at(state).at(side);
        if(keepCurrent){if(source.is_object())source.erase("imageId");}
        else source=id.empty()?json():json{{"asset",Get(id)},{"imageId",id}};
        const auto width=lv_obj_get_style_width(object,LV_PART_MAIN),height=lv_obj_get_style_height(object,LV_PART_MAIN);
        Serialization::ExtendedWidgets::Apply(object,record.at("imagebutton"));lv_obj_set_size(object,width,height);
    }
    size_t Usage(lv_obj_t* root,const std::string& id) const{
        bool used=Selection(root)==id;
        if(lv_obj_get_class(root)==&lv_imagebutton_class){auto record=Serialization::ObjectSerializer::SerializeObject(root);for(const auto& state:record["imagebutton"]["states"])for(const auto& source:state)if(source.is_object()&&source.value("imageId",std::string{})==id)used=true;}
        size_t count=used;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)count+=Usage(lv_obj_get_child(root,i),id);return count;
    }
    void Rebind(lv_obj_t* root){auto id=Selection(root);if(!id.empty())Apply(root,id);
        if(lv_obj_get_class(root)==&lv_imagebutton_class)for(size_t state=0;state<LV_IMAGEBUTTON_STATE_NUM;++state)for(size_t side=0;side<3;++side){auto image=ButtonSelection(root,state,side);if(!image.empty())ApplyButton(root,state,side,image);}
        for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)Rebind(lv_obj_get_child(root,i));
    }
};
