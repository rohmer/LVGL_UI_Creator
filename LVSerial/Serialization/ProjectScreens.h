#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include "ProjectDesign.h"
#include "ProjectFonts.h"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace Serialization::ProjectScreens {
using json=nlohmann::json;
inline bool Multiple(const json& doc){return doc.contains("screens")&&doc["screens"].is_array();}
inline size_t Editing(const json& doc){return Multiple(doc)?doc.value("editorScreen",doc.value("activeScreen",size_t(0))):0;}
inline size_t Count(const json& doc){return Multiple(doc)?doc["screens"].size():1;}
template<class Function> inline void ForEachVariableBinding(json& doc,Function function){
    if(!doc.contains("variables")||!doc["variables"].is_array())return;
    for(auto& item:doc["variables"])if(item.is_object()&&item.contains("var")&&item["var"].is_object()){
        auto& variable=item["var"];
        if(variable.contains("bindings")){
            if(variable["bindings"].is_array())for(auto& binding:variable["bindings"])if(binding.is_object())function(binding);
        }else function(variable);
    }
}
inline void SyncVariableBindingAliases(json& doc){
    if(!doc.contains("variables")||!doc["variables"].is_array())return;
    for(auto& item:doc["variables"])if(item.contains("var")){auto& variable=item["var"];
        if(!variable.is_object()||!variable.contains("bindings")||!variable["bindings"].is_array())continue;
        const auto& bindings=variable["bindings"];
        if(bindings.empty()){variable["oid"]=-1;variable["property"]="";variable.erase("screen");}
        else {variable["oid"]=bindings[0].value("oid",-1);variable["property"]=bindings[0].value("property",std::string{});variable["screen"]=bindings[0].value("screen",size_t(0));}
    }
}
inline std::string UniqueId(const json& doc){
    std::set<std::string> ids;if(Multiple(doc))for(const auto& s:doc["screens"])ids.insert(s.value("id",std::string{}));
    for(unsigned i=1;;++i){auto id="screen-"+std::to_string(i);if(!ids.count(id))return id;}
}
inline void ValidateName(const std::string& name){if(name.empty()||name.size()>120||name.find('\0')!=std::string::npos)throw std::invalid_argument("Screen names must contain 1 to 120 characters");}
inline void EnsureMulti(json& doc){
    if(Multiple(doc))return;
    doc["screens"]=json::array({{{"id","screen-1"},{"name","Screen 1"},{"canvas",doc.value("canvas",json{{"width",320},{"height",240}})},{"obj",doc.value("obj",json::array())}}});
    doc["activeScreen"]=0;doc["editorScreen"]=0;
}
// Editor tools use obj/canvas as the selected-page view. Devices and C export
// use only screens. Sync is explicit so changing selection cannot overwrite it.
inline void SyncAlias(json& doc){
    if(!Multiple(doc))return;
    const auto index=Editing(doc);
    if(index>=doc["screens"].size())throw std::invalid_argument("The selected screen is missing");
    if(doc.contains("obj"))doc["screens"][index]["obj"]=doc["obj"];
    if(doc.contains("canvas"))doc["screens"][index]["canvas"]=doc["canvas"];
}
inline void Select(json& doc,size_t index){
    if(!Multiple(doc)){if(index)throw std::out_of_range("Screen is missing");return;}
    if(index>=doc["screens"].size())throw std::out_of_range("Screen is missing");
    doc["editorScreen"]=index;doc["obj"]=doc["screens"][index].value("obj",json::array());doc["canvas"]=doc["screens"][index].value("canvas",json::object());
}
inline void Normalize(json& doc,bool aliasAuthoritative=false){
    if(!doc.is_object())throw std::invalid_argument("Project must be an object");
    if(doc.contains("screens")&&!doc["screens"].is_array())throw std::invalid_argument("Screens must be an array");
    if(!Multiple(doc))return;
    if(doc["screens"].empty()||doc["screens"].size()>256)throw std::invalid_argument("A project must contain between 1 and 256 screens");
    std::set<std::string> ids;
    for(size_t i=0;i<doc["screens"].size();++i){auto& screen=doc["screens"][i];
        if(!screen.is_object()||!screen.contains("obj")||!screen["obj"].is_array())throw std::invalid_argument("Every screen needs a widget array");
        auto id=screen.value("id",std::string{});if(id.empty()||ids.count(id)){id=UniqueId(doc);screen["id"]=id;}ids.insert(id);
        if(!screen.contains("name"))screen["name"]="Screen "+std::to_string(i+1);
        ValidateName(screen.at("name").get<std::string>());
    }
    auto start=doc.value("activeScreen",size_t(0));if(start>=Count(doc))throw std::invalid_argument("The start screen is missing");
    const auto index=Editing(doc);if(index>=Count(doc))throw std::invalid_argument("The selected screen is missing");
    if(aliasAuthoritative)SyncAlias(doc);
    Select(doc,index);
}
inline void DenseIds(json& doc){
    if(!Multiple(doc))return;
    std::vector<std::map<int,int>> ids(Count(doc));
    for(size_t si=0;si<Count(doc);++si){int next=0;for(const auto& object:doc["screens"][si]["obj"]){const int oid=object.value("oid",next);if(!ids[si].emplace(oid,next++).second)throw std::invalid_argument("Duplicate widget ID");}}
    auto remap=[&](json& id,size_t screen){if(!id.is_number_integer()||id.get<int>()<0||screen>=ids.size())return;auto found=ids[screen].find(id.get<int>());if(found!=ids[screen].end())id=found->second;};
    for(size_t si=0;si<Count(doc);++si)for(auto& object:doc["screens"][si]["obj"]){
        if(object.contains("oid"))remap(object["oid"],si);
        if(object.contains("par"))remap(object["par"],si);
        if(object.contains("advancedKeyboard")&&object["advancedKeyboard"].contains("targetOid"))remap(object["advancedKeyboard"]["targetOid"],si);
        if(object.contains("base")&&object["base"].contains("events"))for(auto& action:object["base"]["events"])if(action.contains("targetOid"))remap(action["targetOid"],action.value("targetScreen",si));
    }
    ForEachVariableBinding(doc,[&](json& binding){if(binding.contains("oid"))remap(binding["oid"],binding.value("screen",size_t(0)));});
    SyncVariableBindingAliases(doc);
    Select(doc,Editing(doc));
}
inline json Canonical(json doc){
    if(Multiple(doc)){SyncAlias(doc);doc.erase("obj");doc.erase("canvas");doc.erase("editorScreen");}
    return doc;
}
inline json Device(json doc){
    doc=Canonical(std::move(doc));
    ProjectDesign::Materialize(doc,true);
    ProjectFonts::PruneUnused(doc);
    for(const auto* key:{"deviceTargets","previewProfiles","accessibility","editorSession"})doc.erase(key);
    // Font conversion inputs are authoring metadata. Devices need only the
    // stable asset identity and embedded glyph data, without desktop paths.
    if(doc.contains("fonts")&&doc["fonts"].is_array())
        for(auto& font:doc["fonts"])if(font.is_object())font.erase("conversion");
    return doc;
}
inline size_t Add(json& doc,const std::string& name){
    ValidateName(name);EnsureMulti(doc);SyncAlias(doc);if(Count(doc)>=256)throw std::invalid_argument("The project already contains 256 screens");
    auto canvas=doc.value("canvas",json{{"width",320},{"height",240}});doc["screens"].push_back({{"id",UniqueId(doc)},{"name",name},{"canvas",std::move(canvas)},{"obj",json::array()},{"transition",{{"type","none"},{"duration",0}}}});
    const auto index=Count(doc)-1;Select(doc,index);return index;
}
inline void Rename(json& doc,size_t index,const std::string& name){ValidateName(name);EnsureMulti(doc);if(index>=Count(doc))throw std::out_of_range("Screen is missing");doc["screens"][index]["name"]=name;}
inline void SetStart(json& doc,size_t index){EnsureMulti(doc);if(index>=Count(doc))throw std::out_of_range("Screen is missing");doc["activeScreen"]=index;}
inline void Remap(json& doc,const std::map<size_t,size_t>& ids){
    auto reference=[&](json& value){if(!value.is_number_integer())return;const auto old=value.get<int64_t>();if(old<0)return;auto found=ids.find(size_t(old));if(found!=ids.end())value=found->second;};
    ForEachVariableBinding(doc,[&](json& binding){if(!binding.contains("screen")&&binding.value("oid",-1)>=0)binding["screen"]=0;if(binding.contains("screen"))reference(binding["screen"]);});
    SyncVariableBindingAliases(doc);
    for(auto& screen:doc["screens"])for(auto& object:screen["obj"])if(object.contains("base")&&object["base"].contains("events"))for(auto& action:object["base"]["events"]){if(action.value("action",std::string{})=="navigate"&&action.contains("screen"))reference(action["screen"]);if(action.contains("targetScreen"))reference(action["targetScreen"]);}
    if(doc.contains("accessibility")&&doc["accessibility"].contains("focusOrder")&&doc["accessibility"]["focusOrder"].is_object()){
        json order=json::object();for(const auto& entry:doc["accessibility"]["focusOrder"].items())try{const auto old=std::stoul(entry.key());if(ids.count(old))order[std::to_string(ids.at(old))]=entry.value();}catch(const std::exception&){}
        doc["accessibility"]["focusOrder"]=std::move(order);
    }
}
inline void Move(json& doc,size_t from,size_t to){
    EnsureMulti(doc);SyncAlias(doc);const auto count=Count(doc);if(from>=count||to>=count)throw std::out_of_range("Screen is missing");if(from==to)return;
    std::vector<size_t> order;for(size_t i=0;i<count;++i)if(i!=from)order.push_back(i);order.insert(order.begin()+to,from);
    std::map<size_t,size_t> ids;json screens=json::array();for(size_t i=0;i<count;++i){ids[order[i]]=i;screens.push_back(doc["screens"][order[i]]);}
    const auto editing=Editing(doc),start=doc.value("activeScreen",size_t(0));doc["screens"]=std::move(screens);Remap(doc,ids);doc["activeScreen"]=ids.at(start);Select(doc,ids.at(editing));
}
inline size_t Duplicate(json& doc,size_t index){
    EnsureMulti(doc);SyncAlias(doc);if(index>=Count(doc))throw std::out_of_range("Screen is missing");if(Count(doc)>=256)throw std::invalid_argument("The project already contains 256 screens");
    auto screen=doc["screens"][index];screen["id"]=UniqueId(doc);screen["name"]=screen.value("name",std::string("Screen"))+" copy";const size_t target=Count(doc);
    std::map<std::string,std::string> variableNames;std::set<std::string> used;
    if(doc.contains("variables"))for(const auto& item:doc["variables"])used.insert(item.at("var").value("name",std::string{}));
    auto variables=doc.value("variables",json::array());
    for(size_t vi=0;vi<variables.size();++vi){const auto& item=variables[vi];const auto& variable=item.at("var");
        if(variable.contains("bindings")){
            if(variable["bindings"].is_array())for(const auto& binding:variable["bindings"])
                if(binding.value("screen",size_t(0))==index&&binding.value("oid",-1)>=0){auto copy=binding;copy["screen"]=target;doc["variables"][vi]["var"]["bindings"].push_back(std::move(copy));}
        }else if(variable.value("screen",size_t(0))==index&&variable.value("oid",-1)>=0){auto copy=item;const auto old=copy["var"].at("name").get<std::string>();auto name=old+"_copy";for(unsigned n=2;used.count(name);++n)name=old+"_copy"+std::to_string(n);used.insert(name);variableNames[old]=name;copy["var"]["name"]=name;copy["var"]["screen"]=target;doc["variables"].push_back(std::move(copy));}
    }
    for(auto& object:screen["obj"])if(object.contains("base")&&object["base"].contains("events"))for(auto& action:object["base"]["events"]){
        if(action.value("action",std::string{})=="navigate"&&action.value("screen",size_t(0))==index)action["screen"]=target;
        if(action.contains("targetScreen")&&action["targetScreen"]==index)action["targetScreen"]=target;
        for(auto* record:{&action,action.contains("condition")?&action["condition"]:nullptr})if(record&&record->is_object()&&record->contains("variable")){const auto name=(*record)["variable"].get<std::string>();if(variableNames.count(name))(*record)["variable"]=variableNames.at(name);}
    }
    SyncVariableBindingAliases(doc);doc["screens"].push_back(std::move(screen));Select(doc,target);return target;
}
inline void Remove(json& doc,size_t index){
    EnsureMulti(doc);SyncAlias(doc);const auto count=Count(doc);if(count<2)throw std::invalid_argument("Keep at least one screen in the project");if(index>=count)throw std::out_of_range("Screen is missing");
    const auto editing=Editing(doc),start=doc.value("activeScreen",size_t(0));
    for(auto& screen:doc["screens"])for(auto& object:screen["obj"])if(object.contains("base")&&object["base"].contains("events")){
        auto& actions=object["base"]["events"];actions.erase(std::remove_if(actions.begin(),actions.end(),[&](const json& action){return (action.value("action",std::string{})=="navigate"&&action.value("screen",size_t(-1))==index)||(action.contains("targetScreen")&&action.at("targetScreen")==index);}),actions.end());
    }
    if(doc.contains("variables"))for(auto& item:doc["variables"]){auto& variable=item.at("var");
        if(variable.contains("bindings")){
            if(variable["bindings"].is_array()){auto& bindings=variable["bindings"];bindings.erase(std::remove_if(bindings.begin(),bindings.end(),[&](const json& binding){return binding.value("screen",size_t(0))==index;}),bindings.end());}
        }else if(variable.value("screen",size_t(0))==index){variable["oid"]=-1;variable["property"]="";variable.erase("screen");}
    }
    doc["screens"].erase(doc["screens"].begin()+index);std::map<size_t,size_t> ids;for(size_t i=0;i<count;++i)if(i!=index)ids[i]=i>index?i-1:i;Remap(doc,ids);
    doc["activeScreen"]=start==index?0:ids.at(start);Select(doc,editing==index?(std::min)(index,count-2):ids.at(editing));
}
}
