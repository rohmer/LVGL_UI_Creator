#pragma once
#include "ImageAsset.h"
#include <set>
#include <map>
#include <stdexcept>

namespace Serialization {
class ProjectImages {
public:
    static json Normalize(const json& images) {
        if(!images.is_array())throw std::invalid_argument("Project images must be an array");
        std::set<std::string> ids,names;
        for(const auto& item:images) {
            const auto id=item.at("id").get<std::string>(),name=item.at("name").get<std::string>();
            if(id.empty()||name.empty()||id.find('\0')!=std::string::npos||name.find('\0')!=std::string::npos||name.find_first_of("\r\n")!=std::string::npos||!ids.insert(id).second||!names.insert(name).second)throw std::invalid_argument("Image IDs and names must be unique and nonempty");
            const auto& image=item.at("image");
            if(!image.is_object()||!image.contains("sourceData")||!image["sourceData"].is_string()||image["sourceData"].get<std::string>().empty())throw std::invalid_argument("Project image has no embedded source");
        }
        return images;
    }
    static void References(json& data,const json& images,bool expand) {
        std::map<std::string,json> byId;std::map<std::string,std::string> byData;
        for(const auto& image:Normalize(images)) {byId[image.at("id")]=image.at("image");byData[image.at("image").dump()]=image.at("id");}
        Walk(data,byId,byData,expand);
    }
private:
    static void Walk(json& data,const std::map<std::string,json>& ids,const std::map<std::string,std::string>& records,bool expand) {
        if(data.is_array()) {for(auto& item:data)Walk(item,ids,records,expand);return;}
        if(!data.is_object())return;
        if(data.contains("imageId")&&!ids.count(data.at("imageId").get<std::string>()))throw std::invalid_argument("Unknown widget image ID");
        if(data.size()==1&&data.contains("projectImage")) {
            const auto id=data.at("projectImage").get<std::string>();auto found=ids.find(id);if(found==ids.end())throw std::invalid_argument("Unknown project image: "+id);
            if(expand) data=found->second;
            return;
        }
        for(auto& item:data.items()) {
            if(item.key()=="images")continue;
            if(!expand&&(item.key()=="asset")&&item.value().is_object()) {
                auto found=records.find(item.value().dump());if(found!=records.end()){item.value()={{"projectImage",found->second}};continue;}
            }
            Walk(item.value(),ids,records,expand);
        }
    }
};
}
