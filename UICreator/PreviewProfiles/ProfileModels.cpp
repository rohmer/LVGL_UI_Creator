#include "ProfileModels.h"
#include <algorithm>
#include <stdexcept>
namespace PreviewProfiles {
using json=nlohmann::json;
nlohmann::json AtResolution(nlohmann::json document,int width,int height)
{
    if(width<1||height<1||width>4096||height>4096||uint64_t(width)*height>8388608)
        throw std::invalid_argument("Preview dimensions must be 1-4096 with at most eight million pixels.");
    auto resize=[&](nlohmann::json& screen){screen["canvas"]["width"]=width;screen["canvas"]["height"]=height;};
    if(document.contains("screens")&&document["screens"].is_array()) {
        const auto current=document.value("editorScreen",document.value("activeScreen",size_t(0)));
        if(current<document["screens"].size()&&document.contains("obj")) {
            document["screens"][current]["obj"]=document["obj"];
            if(document.contains("canvas"))document["screens"][current]["canvas"]=document["canvas"];
        }
        for(auto& screen:document["screens"])resize(screen);
        if(current<document["screens"].size())document["activeScreen"]=current;
    }
    resize(document);
    return document;
}
const json& Presets()
{
    static const json value=json::array({
        {{"id","qvga"},{"name","ESP32 QVGA"},{"width",320},{"height",240}},
        {{"id","hvga"},{"name","ESP32 landscape"},{"width",480},{"height",320}},
        {{"id","wvga"},{"name","Wide touch display"},{"width",800},{"height",480}},
        {{"id","pi-small"},{"name","Raspberry Pi compact"},{"width",1024},{"height",600}},
        {{"id","pi-hd"},{"name","Raspberry Pi HD"},{"width",1280},{"height",720}},
        {{"id","portrait"},{"name","Portrait touch display"},{"width",480},{"height",800}}
    });return value;
}
json Profiles(const json& document)
{
    auto result=Presets();const auto metadata=document.value("previewProfiles",json::object());
    if(metadata.is_object()&&metadata.contains("custom")&&metadata["custom"].is_array())for(const auto& item:metadata["custom"]) {
        if(!item.is_object()||!item.contains("name")||!item["name"].is_string()||!item.contains("id")||!item["id"].is_string()||!item.contains("width")||!item["width"].is_number_integer()||!item.contains("height")||!item["height"].is_number_integer())continue;
        if(item["width"].get<double>()<1||item["width"].get<double>()>4096||item["height"].get<double>()<1||item["height"].get<double>()>4096)continue;
        const int w=item.value("width",0),h=item.value("height",0);if(w<1||w>4096||h<1||h>4096||uint64_t(w)*h>8388608)continue;
        result.push_back(item);
    }
    return result;
}
void SaveProfile(json& document,const std::string& name,int width,int height)
{
    if(name.empty()||name.size()>80||name.find_first_of("\r\n\t")!=std::string::npos)throw std::invalid_argument("Enter a profile name of 1-80 characters");
    AtResolution(json::object(),width,height);
    auto& metadata=document["previewProfiles"];if(!metadata.is_object())metadata=json::object();metadata["version"]=1;auto& custom=metadata["custom"];if(!custom.is_array())custom=json::array();
    for(auto& item:custom)if(item.value("name",std::string())==name){item["width"]=width;item["height"]=height;return;}
    if(custom.size()>=32)throw std::invalid_argument("A project supports up to 32 custom preview profiles");
    std::string id;unsigned index=1;do{id="custom-"+std::to_string(index++);}while(std::any_of(custom.begin(),custom.end(),[&](const json& item){return item.value("id",std::string())==id;}));
    custom.push_back({{"id",id},{"name",name},{"width",width},{"height",height}});
}
}
